#include <common/track.h>
#include <ecal/hit.h>
#include <grain/hit.h>
#include <tracker/hit.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <cmath>
#include <cstddef>
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
  // that all values match the csv recorded by the fill run.
  struct track_hits_resolve : public ufw::process {
    track_hits_resolve();
    void run() override;
  };

  namespace {

    const std::string csv_path = "test/track_hits.csv";
    const double tol           = 1.e-6;

    struct expected_hit {
      double cx, cy, cz, ax, ay, az, t_earliest, t_best, t_latest, width;
      std::string dummy;
    };

    // (context, store, index) -> expected values, as written by the fill run.
    using csv_map = std::map<std::tuple<std::size_t, std::string, std::size_t>, expected_hit>;

    const csv_map& load_csv() {
      static const csv_map csv = [] {
        csv_map m;
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
          e.cx                    = std::stod(next());
          e.cy                    = std::stod(next());
          e.cz                    = std::stod(next());
          e.ax                    = std::stod(next());
          e.ay                    = std::stod(next());
          e.az                    = std::stod(next());
          e.t_earliest            = std::stod(next());
          e.t_best                = std::stod(next());
          e.t_latest              = std::stod(next());
          e.width                 = std::stod(next());
          e.dummy                 = next();
          m.emplace(std::make_tuple(ctx, store, idx), e);
        }
        return m;
      }();
      return csv;
    }

    void close_enough(double a, double b, const std::string& what) {
      UFW_ASSERT(std::abs(a - b) < tol, "The {} must match the csv: {} vs {}.", what, a, b);
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
        close_enough(h.centre().x(), e.cx, "centre.x");
        close_enough(h.centre().y(), e.cy, "centre.y");
        close_enough(h.centre().z(), e.cz, "centre.z");
        close_enough(h.axis().x(), e.ax, "axis.x");
        close_enough(h.axis().y(), e.ay, "axis.y");
        close_enough(h.axis().z(), e.az, "axis.z");
        close_enough(h.t().earliest(), e.t_earliest, "time.earliest");
        close_enough(h.t().best(), e.t_best, "time.best");
        close_enough(h.t().latest(), e.t_latest, "time.latest");
        close_enough(h.width(), e.width, "width");
        check_dummy(h, e);
      }
      total += n;
    };

    check_store("grain", gh, tr.grain_hits(), [](const auto& h, const expected_hit& e) {
      UFW_ASSERT(std::abs(h.test_float() - std::stod(e.dummy)) < 1.e-6,
                 "The grain dummy must match the csv: {} vs {}.",
                 h.test_float(),
                 e.dummy);
    });
    check_store("tracker", th, tr.tracker_hits(), [](const auto& h, const expected_hit& e) {
      UFW_ASSERT(std::abs(h.test_double() - std::stod(e.dummy)) < 1.e-6,
                 "The tracker dummy must match the csv: {} vs {}.",
                 h.test_double(),
                 e.dummy);
    });
    check_store("ecal", eh, tr.ecal_hits(), [](const auto& h, const expected_hit& e) {
      UFW_ASSERT(h.test_string() == e.dummy,
                 "The ecal dummy must match the csv: {} vs {}.",
                 h.test_string(),
                 e.dummy);
    });

    // Through the single-container interface, the track must visit every hit of its stores.
    std::size_t visited = 0;
    tr.for_each_hit([&](const reco::hit&) { ++visited; });
    UFW_ASSERT(visited == total, "for_each_hit must visit all {} hits, was {}.", total, visited);
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::track_hits_resolve)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::track_hits_resolve)
