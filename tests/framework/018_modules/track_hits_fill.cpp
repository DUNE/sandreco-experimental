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
#include <fstream>
#include <iomanip>
#include <numeric>
#include <random>
#include <sstream>

namespace sand::test {

  // Fills the three hit stores with randomized stub hits and a track referencing every hit in a
  // shuffled order (see 018_track_hits_write.json), and records all values in a csv file so that
  // 019_track_hits_read.json can verify the round trip through the root file.
  struct track_hits_fill : public ufw::process {
    track_hits_fill();
    void run() override;
  };

  namespace {

    const std::string csv_path  = "test/track_hits.csv";
    const std::size_t n_grain   = 8;
    const std::size_t n_tracker = 6;
    const std::size_t n_ecal    = 6;

    std::string fmt(double v) {
      std::ostringstream oss;
      oss << std::setprecision(15) << v;
      return oss.str();
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
    csv << std::setprecision(15);
    if (ctx == 0) {
      csv << "context,store,index,cx,cy,cz,ax,ay,az,t_earliest,t_best,t_latest,width,dummy\n";
    }

    // Write one csv row for each hit as it is pushed.
    auto emit = [&](const char* store_name, std::size_t i, const reco::hit& h, const std::string& dummy) {
      const auto c = h.centre();
      const auto a = h.axis();
      const auto t = h.t();
      std::ostringstream oss;
      oss << std::setprecision(15) << ctx << "," << store_name << "," << i << ',' << c.x() << ',' << c.y() << ','
          << c.z() << ',' << a.x() << ',' << a.y() << ',' << a.z() << ',' << t.earliest() << ',' << t.best() << ','
          << t.latest() << ',' << h.width() << ',' << dummy << "\n";
      csv << oss.str();
    };

    for (std::size_t i = 0; i < n_grain; ++i) {
      auto h = sand::grain::hit{pos_3d(real(gen), real(gen), real(gen)),
                                dir_3d(real(gen), real(gen), real(gen)),
                                reco::timerange(real(gen), 1.0),
                                realw(gen),
                                realf(gen)};
      gh.push_back(h);
      emit("grain", i, h, fmt(h.test_float()));
    }
    for (std::size_t i = 0; i < n_tracker; ++i) {
      auto h = sand::tracker::hit{pos_3d(real(gen), real(gen), real(gen)),
                                  dir_3d(real(gen), real(gen), real(gen)),
                                  reco::timerange(real(gen), 1.0),
                                  realw(gen),
                                  real(gen)};
      th.push_back(h);
      emit("tracker", i, h, fmt(h.test_double()));
    }
    for (std::size_t i = 0; i < n_ecal; ++i) {
      auto h = sand::ecal::hit{pos_3d(real(gen), real(gen), real(gen)),
                               dir_3d(real(gen), real(gen), real(gen)),
                               reco::timerange(real(gen), 1.0),
                               realw(gen),
                               std::to_string(string_id(gen))};
      eh.push_back(h);
      emit("ecal", i, h, h.test_string());
    }

    // Assemble the track: every pushed hit is referenced exactly once, in a random
    // (shuffled) order within each store's key vector.
    auto shuffled = [&](std::size_t n) {
      std::vector<std::size_t> perm(n);
      std::iota(perm.begin(), perm.end(), 0);
      std::shuffle(perm.begin(), perm.end(), gen);
      return perm;
    };
    auto& tr = set<reco::track>("track");
    for (const auto i : shuffled(n_grain)) {
      tr.push(sand::grain::grain_hits_index{i});
    }
    for (const auto i : shuffled(n_tracker)) {
      tr.push(sand::tracker::tracker_hits_index{i});
    }
    for (const auto i : shuffled(n_ecal)) {
      tr.push(sand::ecal::ecal_hits_index{i});
    }
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::track_hits_fill)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::track_hits_fill)
