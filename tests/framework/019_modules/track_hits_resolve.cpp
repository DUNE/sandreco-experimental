#include <common/track.h>
#include <ecal/hit.h>
#include <grain/hit.h>
#include <tracker/hit.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace sand::test {

  // Reads back the track and hit stores written by 018_track_hits_write.json and verifies, through
  // the track interface, that every key resolves to the very element of the read-back store and
  // that all values match the csv recorded by the fill run bit by bit (the numeric columns of the
  // csv are the bit patterns of the values).
  //
  // Structural problems (invalid keys, non-permutation key order, keys resolving to the wrong
  // element, missing csv rows) fail immediately; value mismatches are counted per field over the
  // whole context and reported in a single summary, so the failure log shows which members are
  // lost (as in 205_ecal_digi_streamer_read).
  struct track_hits_resolve : public ufw::process {
    track_hits_resolve();
    void run() override;
  };

  namespace {

    const std::string csv_path = "test/track_hits.csv";

    struct expected_hit {
      // Bit patterns of the numeric members, exactly as written by the fill run.
      std::uint64_t cx, cy, cz, ax, ay, az, t_earliest, t_best, t_latest, width;
      std::string dummy;
    };

    // (context, store, index) -> expected values, as written by the fill run.
    using csv_map = std::map<std::tuple<std::size_t, std::string, std::size_t>, expected_hit>;

    const csv_map& load_csv() {
      static const csv_map csv = [] {
        csv_map m;
        // Per-store rows, rows with a nonzero width, rows with a nonzero dummy, rows with a
        // nonempty dummy: the test is only meaningful if the fill recorded at least one of each,
        // so a systematically dropped member cannot yield a vacuous pass.
        std::map<std::string, std::array<std::size_t, 4>> stats;
        // Same path convention as the streamer's root file: relative paths expand against the basepath.
        const auto csv_file = ufw::config::expand_path(csv_path, ufw::config::io::read | ufw::config::io::file);
        std::ifstream in(csv_file);
        UFW_ASSERT(in, "The csv file {} written by the fill run must be readable.", csv_file.string());
        std::string line;
        while (std::getline(in, line)) {
          if (line.rfind("context,", 0) == 0) {
            continue; // header
          }
          std::istringstream iss(line);
          std::string cell;
          auto next = [&]() {
            std::getline(iss, cell, ',');
            return cell;
          };
          expected_hit e;
          const std::size_t ctx   = std::stoull(next());
          const std::string store = next();
          const std::size_t idx   = std::stoull(next());
          e.cx                    = std::stoull(next());
          e.cy                    = std::stoull(next());
          e.cz                    = std::stoull(next());
          e.ax                    = std::stoull(next());
          e.ay                    = std::stoull(next());
          e.az                    = std::stoull(next());
          e.t_earliest            = std::stoull(next());
          e.t_best                = std::stoull(next());
          e.t_latest              = std::stoull(next());
          e.width                 = std::stoull(next());
          e.dummy                 = next();
          UFW_ASSERT(!iss.fail(), "Malformed csv line '{}'.", line);
          const auto [it, ok] = m.emplace(std::make_tuple(ctx, store, idx), e);
          UFW_ASSERT(ok, "The csv must not contain a duplicate row for context {} store {} index {}.", ctx, store, idx);
          auto& s           = stats[store];
          s[0] += 1;
          s[1] += e.width != 0 ? 1 : 0;
          s[2] += e.dummy != "0" ? 1 : 0;
          s[3] += !e.dummy.empty() ? 1 : 0;
        }
        for (const auto store : {"grain", "tracker", "ecal"}) {
          const auto it = stats.find(store);
          UFW_ASSERT(it != stats.end(), "The csv must contain rows for the {} store.", store);
          const auto& s = it->second;
          UFW_ASSERT(s[1] > 0, "The csv must contain at least one {} hit with a nonzero width.", store);
          UFW_ASSERT(s[2] > 0, "The csv must contain at least one {} hit with a nonzero dummy value.", store);
          UFW_ASSERT(s[3] > 0, "The csv must contain at least one {} hit with a nonempty dummy value.", store);
        }
        return m;
      }();
      return csv;
    }

    // Bit pattern of the value, the same encoding the fill run used for the csv.
    template <typename T>
    std::uint64_t pattern(T v) {
      std::uint64_t b = 0;
      std::memcpy(&b, &v, sizeof(v));
      return b;
    }

    // Counts the mismatches of each field over the context, so the summary shows which
    // members are lost, then fails once with the summary.
    struct field_errors {
      std::map<std::string, std::size_t> count;
      void add(const std::string& what) {
        ++count[what];
      }
      bool empty() const { return count.empty(); }
      std::string summary() const {
        std::string s;
        for (const auto& [f, n] : count) {
          s += f + ": " + std::to_string(n) + "; ";
        }
        return s;
      }
    };

    void same(field_errors& errs, std::uint64_t got, std::uint64_t ref, const std::string& what) {
      if (got != ref) {
        errs.add(what);
      }
    }

  } // namespace

  track_hits_resolve::track_hits_resolve()
    : process({{"track", "sand::reco::track"},
               {"grain_hits", "sand::grain::grain_hits"},
               {"tracker_hits", "sand::tracker::tracker_hits"},
               {"ecal_hits", "sand::ecal::ecal_hits"}},
              {}) {}

  void track_hits_resolve::run() {
    const auto ctx  = ufw::context::current()->id();
    const auto& csv = load_csv();
    // Touch the track first, before the stores are ever used: this exercises the late-attach
    // read path, since a store branch is only attached (and re-read) on first key resolution below.
    const auto& tr = get<reco::track>("track");
    const auto& gh = sand::grain::grain_hits_store();
    const auto& th = sand::tracker::tracker_hits_store();
    const auto& eh = sand::ecal::ecal_hits_store();

    std::size_t total = 0;
    field_errors errs;
    // Check that the keys of one store form a permutation of [0, n), that every key
    // resolves to the very element of the read-back store, and that all values match the csv.
    auto check_store = [&](const std::string& name, const auto& store, const auto& keys, auto check_dummy) {
      const std::size_t n = store.size();
      UFW_ASSERT(keys.size() == n, "The track must reference all {} {} hits, was {}.", n, name, keys.size());
      std::cout << "track " << name << " key order: ";
      for (const auto& k : keys) {
        std::cout << k.get() << ' ';
      }
      std::cout << "\n";
      std::vector<bool> seen(n, false);
      for (const auto& k : keys) {
        UFW_ASSERT(static_cast<bool>(k), "The track's {} key must be valid after read-back.", name);
        const std::size_t i = k.get();
        UFW_ASSERT(i < n && !seen[i], "The track's {} keys must be a permutation of [0, {}), index {} is not.",
                   name,
                   n,
                   i);
        seen[i] = true;
        UFW_ASSERT(&*k == &store.at(i), "The {} key must resolve to element {} of the read-back store.", name, i);
        const auto it = csv.find(std::make_tuple(ctx, name, i));
        UFW_ASSERT(it != csv.end(), "The csv must contain a row for context {} store {} index {}.", ctx, name, i);
        const auto& e = it->second;
        const auto& h = *k;
        same(errs, pattern(h.centre().x()), e.cx, name + ".centre.x");
        same(errs, pattern(h.centre().y()), e.cy, name + ".centre.y");
        same(errs, pattern(h.centre().z()), e.cz, name + ".centre.z");
        same(errs, pattern(h.axis().x()), e.ax, name + ".axis.x");
        same(errs, pattern(h.axis().y()), e.ay, name + ".axis.y");
        same(errs, pattern(h.axis().z()), e.az, name + ".axis.z");
        same(errs, pattern(h.t().earliest()), e.t_earliest, name + ".time.earliest");
        same(errs, pattern(h.t().best()), e.t_best, name + ".time.best");
        same(errs, pattern(h.t().latest()), e.t_latest, name + ".time.latest");
        same(errs, pattern(h.width()), e.width, name + ".width");
        check_dummy(h, e, errs);
      }
      total += n;
    };

    check_store("grain", gh, tr.grain_hits(), [](const auto& h, const expected_hit& e, field_errors& errs) {
      same(errs, pattern(h.test_float()), std::stoull(e.dummy), "grain.test_float");
    });
    check_store("tracker", th, tr.tracker_hits(), [](const auto& h, const expected_hit& e, field_errors& errs) {
      same(errs, pattern(h.test_double()), std::stoull(e.dummy), "tracker.test_double");
    });
    check_store("ecal", eh, tr.ecal_hits(), [](const auto& h, const expected_hit& e, field_errors& errs) {
      if (h.test_string() != e.dummy) {
        errs.add("ecal.test_string");
      }
    });

    // Guard against a vacuous pass: the read-back stores must not be empty.
    UFW_ASSERT(total > 0, "The track's hit stores must not read back empty.");

    // Through the single-container interface, the track must visit every hit of its stores.
    std::size_t visited = 0;
    tr.for_each_hit([&](const reco::hit&) { ++visited; });
    UFW_ASSERT(visited == total, "for_each_hit must visit all {} hits, was {}.", total, visited);

    // Report the value mismatches, if any, in one summary: which members are lost, and how often.
    UFW_ASSERT(errs.empty(), "Value mismatches against the csv: {}", errs.summary());
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::track_hits_resolve)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::track_hits_resolve)
