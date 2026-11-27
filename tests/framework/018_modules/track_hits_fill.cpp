#include <common/track.h>
#include <ecal/hit.h>
#include <grain/hit.h>
#include <tracker/hit.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace sand::test {

  // Fills the three hit stores with randomized stub hits and a track referencing every hit in a
  // shuffled order (see 018_track_hits_write.json), and records all values in a csv file so that
  // 019_track_hits_read.json can verify the round trip through the root file.
  //
  // The numeric columns hold the bit pattern of the value, so the read-back values can be
  // compared bit by bit instead of within a tolerance (doubles printed in decimal would need 17
  // significant digits to round-trip exactly). The ecal dummy is a string member and is written
  // verbatim; the grain and tracker dummies are a float and a double, encoded like the others.
  struct track_hits_fill : public ufw::process {
    track_hits_fill();
    void run() override;
  };

  namespace {

    const std::string csv_path  = "test/track_hits.csv";
    const std::size_t n_grain   = 8;
    const std::size_t n_tracker = 6;
    const std::size_t n_ecal    = 6;

    std::string bits(double v) {
      std::uint64_t b = 0;
      std::memcpy(&b, &v, sizeof(b));
      return std::to_string(b);
    }

    std::string bits(float v) {
      std::uint32_t b = 0;
      std::memcpy(&b, &v, sizeof(b));
      return std::to_string(b);
    }

  } // namespace

  track_hits_fill::track_hits_fill()
    : process({}, {{"track", "sand::reco::track"},
                   {"grain_hits", "sand::grain::grain_hits"},
                   {"tracker_hits", "sand::tracker::tracker_hits"},
                   {"ecal_hits", "sand::ecal::ecal_hits"}}) {}

  void track_hits_fill::run() {
    const auto ctx = ufw::context::current()->id();
    auto& gh       = sand::grain::grain_hits_store();
    auto& th       = sand::tracker::tracker_hits_store();
    auto& eh       = sand::ecal::ecal_hits_store();
    UFW_ASSERT(gh.empty() && th.empty() && eh.empty(), "The hit stores must be reset between contexts.");

    std::mt19937 gen(12345 + 1000u * ctx);
    std::uniform_real_distribution<double> real(0., 100.);
    std::uniform_real_distribution<float> realf(0.f, 100.f);
    std::uniform_real_distribution<double> realw(0., 10.);
    std::uniform_int_distribution<int> string_id(0, 999999);

    // Same path convention as the streamer's root file: relative paths expand against the basepath.
    const auto csv_file = ufw::config::expand_path(csv_path, ufw::config::io::write | ufw::config::io::file);
    std::ofstream csv(csv_file, ctx == 0 ? std::ios::trunc : std::ios::app);
    UFW_ASSERT(csv, "The csv file {} could not be opened for writing.", csv_file.string());
    if (ctx == 0) {
      csv << "context,store,index,cx,cy,cz,ax,ay,az,t_earliest,t_best,t_latest,width,dummy\n";
    }

    // Write one csv row for each hit as it is pushed.
    auto emit = [&](const char* store_name, std::size_t i, const reco::hit& h, const std::string& dummy) {
      const auto c = h.centre();
      const auto a = h.axis();
      const auto t = h.t();
      csv << ctx << ',' << store_name << ',' << i << ',' << bits(c.x()) << ',' << bits(c.y()) << ',' << bits(c.z()) << ','
          << bits(a.x()) << ',' << bits(a.y()) << ',' << bits(a.z()) << ',' << bits(t.earliest()) << ',' << bits(t.best())
          << ',' << bits(t.latest()) << ',' << bits(h.width()) << ',' << dummy << "\n";
    };

    // Fill the stores, collecting the keys minted by the insertions (the only way to get them).
    std::vector<sand::grain::grain_hits_index> gkeys;
    for (std::size_t i = 0; i < n_grain; ++i) {
      auto h = sand::grain::hit{pos_3d(real(gen), real(gen), real(gen)),
                                dir_3d(real(gen), real(gen), real(gen)),
                                reco::timerange(real(gen), 1.0),
                                realw(gen),
                                realf(gen)};
      gkeys.push_back(gh.push_back(h));
      emit("grain", i, h, bits(h.test_float()));
    }
    std::vector<sand::tracker::tracker_hits_index> tkeys;
    for (std::size_t i = 0; i < n_tracker; ++i) {
      auto h = sand::tracker::hit{pos_3d(real(gen), real(gen), real(gen)),
                                  dir_3d(real(gen), real(gen), real(gen)),
                                  reco::timerange(real(gen), 1.0),
                                  realw(gen),
                                  real(gen)};
      tkeys.push_back(th.push_back(h));
      emit("tracker", i, h, bits(h.test_double()));
    }
    std::vector<sand::ecal::ecal_hits_index> ekeys;
    for (std::size_t i = 0; i < n_ecal; ++i) {
      auto h = sand::ecal::hit{pos_3d(real(gen), real(gen), real(gen)),
                               dir_3d(real(gen), real(gen), real(gen)),
                               reco::timerange(real(gen), 1.0),
                               realw(gen),
                               std::to_string(string_id(gen))};
      ekeys.push_back(eh.push_back(h));
      emit("ecal", i, h, h.test_string());
    }

    // Assemble the track: every pushed hit is referenced exactly once, in a random
    // (shuffled) order within each store's key vector.
    auto shuffle_keys = [&](auto& keys) { std::shuffle(keys.begin(), keys.end(), gen); };
    auto& tr          = set<reco::track>("track");
    shuffle_keys(gkeys);
    for (const auto& k : gkeys) {
      tr.push(k);
    }
    shuffle_keys(tkeys);
    for (const auto& k : tkeys) {
      tr.push(k);
    }
    shuffle_keys(ekeys);
    for (const auto& k : ekeys) {
      tr.push(k);
    }
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::track_hits_fill)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::track_hits_fill)
