#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>

#include <ecal/cluster.h>
#include <geoinfo/ecal_info.hpp>
#include <geoinfo/geoinfo.hpp>
#include <processes/ecal/preclustering/preclustering.hpp>

#include <ufw/factory.hpp>
#include <ufw/process.hpp>

namespace sand::test {

  namespace {

    using cell_id = sand::geoinfo::ecal_info::cell_id;

    cell_id id(geo_id::region_t region, std::uint8_t module, std::uint8_t row, std::uint8_t column) {
      cell_id result;
      result.region        = region;
      result.module_number = module;
      result.row           = row;
      result.column        = column;
      return result;
    }

    bool close(double actual, double expected) {
      return std::isfinite(actual) && std::abs(actual - expected) <= 1.e-10 * std::max(1.0, std::abs(expected));
    }

    void check_window(const ecal::cluster& cluster, double earliest, double best, double latest) {
      UFW_ASSERT(cluster.digit_window.earliest() == earliest && cluster.digit_window.best() == best
                     && cluster.digit_window.latest() == latest,
                 "Precluster lost its original digit window");
    }

  } // namespace

  // Integration cases with controlled reco_cells and explicit precluster memberships and observables.
  class ecal_preclustering : public ufw::process {
   public:
    ecal_preclustering()
      : process({}, {{"reco_cells", "sand::ecal::reco_cells"},
                     {"timeranges", "sand::reco::timeranges"},
                     {"clusters", "sand::ecal::clusters"}}) {}

    void run() override {
      const auto& geometry = instance<sand::geoinfo>().ecal();
      auto& cells          = instance<ecal::reco_cells>("preclustering_test_cells");
      auto& windows        = instance<reco::timeranges>("preclustering_test_windows");
      const auto& clusters = instance<ecal::clusters>("preclustering_test_clusters");
      ecal::preclustering subject;
      subject.setup({{"reco_cells", "preclustering_test_cells"}, {"timeranges", "preclustering_test_windows"}},
                    {{"clusters", "preclustering_test_clusters"}});

      auto make_cell = [&](cell_id cid, double tdc, double energy = 1.0) {
        using face           = sand::geoinfo::ecal_info::face_location;
        const auto& physical = geometry.at(cid);
        ecal::reco_cell cell;
        cell.begin         = ecal::digit(geometry.channel({cid, face::begin}), tdc, 10.0, 0.0);
        cell.end           = ecal::digit(geometry.channel({cid, face::end}), tdc, 10.0, 0.0);
        cell.begin_present = cell.end_present = true;
        cell.n_complete_candidates_in_cell    = 1;
        cell.position                         = physical.offset2position(0.0);
        cell.time = reco::timerange(tdc - physical.total_pathlength() / (2.0 * physical.get_fiber().light_velocity));
        cell.e    = energy;
        return cell;
      };

      UFW_ASSERT(std::isnan(ecal::cluster{}.time_span), "Default cluster time span must be unset");

      std::size_t cases = 0; 
      auto run_case     = [&](std::initializer_list<ecal::reco_cell> input,
                          std::initializer_list<reco::timerange> time_windows) {
        cells.assign(input);
        windows.assign(time_windows);
        subject.run();
        for (const auto& cluster : clusters) {
          if (cluster.cells.size() == 1) {
            UFW_ASSERT(cluster.time_span == 0.0, "Singleton cluster has a nonzero time span");
          }
        }
        ++cases;
      };
      auto expect_failure = [&](std::initializer_list<ecal::reco_cell> input,
                                std::initializer_list<reco::timerange> time_windows) {
        cells.assign(input);
        windows.assign(time_windows);
        bool rejected = false;
        try {
          subject.run();
        } catch (const ufw::exception&) {
          rejected = true;
        }
        UFW_ASSERT(rejected, "Preclustering accepted malformed input");
        ++cases;
      };

      const auto a = id(geo_id::BARREL, 0, 0, 0);
      const auto b = id(geo_id::BARREL, 0, 0, 1);
      const auto c = id(geo_id::BARREL, 0, 0, 2);
      const reco::timerange full(150.0, 100.0, 200.0);

      // Check energy-weighted observables and the Neff variance correction.
      // Override reconstructed positions to verify connectivity depends on physical IDs.
      auto left      = make_cell(a, 120.0, 2.0);
      auto right     = make_cell(b, 130.0, 6.0);
      left.position  = pos_3d(10.0, 20.0, 30.0);
      right.position = pos_3d(30.0, 40.0, 50.0);
      left.time      = reco::timerange(20.0);
      right.time     = reco::timerange(40.0); 
      run_case({right, left}, {full});
      UFW_ASSERT(clusters.size() == 1 && clusters[0].cells.size() == 2, "Adjacent cells did not form one cluster");
      const auto& weighted = clusters[0];
      UFW_ASSERT(close(weighted.e, 8.0) && close(weighted.centre().x(), 25.0) && close(weighted.centre().y(), 35.0)
                     && close(weighted.centre().z(), 45.0) && close(weighted.t().best(), 35.0)
                     && close(weighted.t().earliest(), 34.9) && close(weighted.t().latest(), 35.1),
                 "Incorrect energy-weighted cluster observables");
      UFW_ASSERT(close(weighted.time_span, 20.0), "Precluster time span must use reconstructed best times");
      for (const auto sigma : weighted.sigma) {
        UFW_ASSERT(close(sigma, std::sqrt(200.0)), "Incorrect effective-sample correction");
      }
      UFW_ASSERT(std::isnan(weighted.axis().x()) && std::isnan(weighted.width()) && std::isnan(weighted.apex.x())
                     && weighted.fit == ecal::cluster::fit_status::not_run,
                 "Preclustering must leave fitted observables unset");
      check_window(weighted, 100.0, 150.0, 200.0);
      UFW_ASSERT(cells[0].begin.tdc() == 130.0 && cells[1].begin.tdc() == 120.0, "Input order was modified");

      // Verify transitive connectivity along the A-B-C neighbour chain.
      const auto chain_left   = make_cell(a, 110.0);
      const auto chain_middle = make_cell(b, 120.0);
      const auto chain_right  = make_cell(c, 130.0);
      const double chain_span = std::max({chain_left.time.best(), chain_middle.time.best(), chain_right.time.best()})
                              - std::min({chain_left.time.best(), chain_middle.time.best(), chain_right.time.best()});
      run_case({chain_right, chain_left, chain_middle}, {full});
      UFW_ASSERT(clusters.size() == 1 && clusters[0].cells.size() == 3, "Transitive neighbour chain was broken");
      UFW_ASSERT(close(clusters[0].time_span, chain_span), "Neighbour chain lost its full reconstructed-time span");

      // Missing or filtered B must separate A and C.
      run_case({make_cell(a, 110.0), make_cell(c, 130.0)}, {full});
      UFW_ASSERT(clusters.size() == 2, "An empty cell bridged two components");
      for (const auto& cluster : clusters) {
        UFW_ASSERT(cluster.cells.size() == 1 && cluster.sigma[0] == 0.0 && cluster.sigma[1] == 0.0
                       && cluster.sigma[2] == 0.0,
                   "Singleton cluster has a nonzero spread");
      }
      run_case({make_cell(a, 110.0), make_cell(b, 120.0, 0.0), make_cell(c, 130.0)}, {full});
      UFW_ASSERT(clusters.size() == 2, "Zero-energy cell bridged two components");
      auto excluded                              = make_cell(b, 120.0);
      excluded.cell_has_competing_complete_pairs = true;
      run_case({make_cell(a, 110.0), excluded, make_cell(c, 130.0)}, {full});
      UFW_ASSERT(clusters.size() == 2, "Ambiguous cell bridged two components");
      excluded             = make_cell(b, 120.0);
      excluded.end_present = false;
      run_case({make_cell(a, 110.0), excluded, make_cell(c, 130.0)}, {full});
      UFW_ASSERT(clusters.size() == 2, "Incomplete cell bridged two components");
      excluded                       = make_cell(b, 120.0);
      excluded.originally_incomplete = true;
      run_case({make_cell(a, 110.0), excluded, make_cell(c, 130.0)}, {full});
      UFW_ASSERT(clusters.size() == 2, "Originally incomplete cell bridged two components");

      // Digit TDCs enforce half-open membership independently of reconstructed times.
      // Windows without reconstructed cells must not produce preclusters.
      auto early = make_cell(a, 50.0);
      auto late  = make_cell(b, 100.0);
      early.time = reco::timerange(120.0);
      late.time  = reco::timerange(80.0);
      run_case({late, early}, {reco::timerange(50.0, 0.0, 100.0), reco::timerange(150.0, 100.0, 200.0),
                               reco::timerange(250.0, 200.0, 300.0)});
      UFW_ASSERT(clusters.size() == 2 && clusters[0].cells.size() == 1 && clusters[1].cells.size() == 1,
                 "Adjacent cells from different windows were joined");
      check_window(clusters[0], 0.0, 50.0, 100.0);
      check_window(clusters[1], 100.0, 150.0, 200.0);
      UFW_ASSERT(clusters[0].t().best() == 120.0 && clusters[1].t().best() == 80.0,
                 "Digit windows changed the reconstructed particle times");

      // Check gaps, unpopulated windows and half-open boundary membership.
      run_case({make_cell(a, -1.0), make_cell(a, 0.0), make_cell(b, 100.0), make_cell(b, 150.0), make_cell(b, 300.0),
                make_cell(c, 400.0)},
               {reco::timerange(50.0, 0.0, 100.0), reco::timerange(250.0, 200.0, 300.0),
                reco::timerange(350.0, 300.0, 400.0)});
      UFW_ASSERT(clusters.size() == 2 && clusters[0].cells.size() == 1 && clusters[1].cells.size() == 1,
                 "Gap or half-open window membership is incorrect");
      check_window(clusters[1], 300.0, 350.0, 400.0);

      run_case({}, {full});
      UFW_ASSERT(clusters.empty(), "Empty input retained clusters from the previous run");
      run_case({make_cell(a, 120.0)}, {});
      UFW_ASSERT(clusters.empty(), "Absent windows must produce no clusters");

      run_case({make_cell(a, 110.0), make_cell(a, 130.0)}, {full});
      UFW_ASSERT(clusters.size() == 1 && clusters[0].cells.size() == 2,
                 "Repeated physical ID lost a reconstructed cell");
      UFW_ASSERT(close(clusters[0].time_span, 20.0), "Repeated physical ID lost its reconstructed-time span");
      run_case({make_cell(a, 120.0), make_cell(id(geo_id::ENDCAP_A, 2, 0, 0), 120.0),
                make_cell(id(geo_id::ENDCAP_B, 2, 0, 0), 120.0)},
               {full});
      UFW_ASSERT(clusters.size() == 3, "Different detector regions were joined");
      UFW_ASSERT(std::count_if(clusters.begin(), clusters.end(),
                               [](const auto& cluster) { return cluster.type == ecal::cluster::topology::endcap; })
                     == 2,
                 "Incorrect detector topology");
      run_case({make_cell(id(geo_id::BARREL, 23, 0, 11), 120.0), make_cell(a, 120.0)}, {full});
      UFW_ASSERT(clusters.size() == 1, "Barrel wraparound neighbours were separated");
      run_case({make_cell(id(geo_id::ENDCAP_A, 1, 0, 5), 120.0), make_cell(id(geo_id::ENDCAP_A, 2, 0, 2), 120.0)},
               {full});
      UFW_ASSERT(clusters.size() == 1, "Endcap module-boundary neighbours were separated");

      // Check variance stability for extreme energy ratios.
      left.e  = 1.e300;
      right.e = 1.e-300;
      run_case({left, right}, {full});
      UFW_ASSERT(close(clusters[0].sigma[0], std::sqrt(200.0)), "Unequal energies destabilized the spatial spread");
      UFW_ASSERT(close(clusters[0].time_span, 20.0), "Unequal energies changed the unweighted time span");

      auto invalid = make_cell(a, 120.0, -1.0);
      expect_failure({invalid}, {full});
      invalid          = make_cell(a, 120.0);
      invalid.position = pos_3d(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0);
      expect_failure({invalid}, {full});
      invalid     = make_cell(a, 90.0);
      invalid.end = make_cell(a, 100.0).end;
      expect_failure({invalid}, {reco::timerange(50.0, 0.0, 100.0), reco::timerange(150.0, 100.0, 200.0)});
      expect_failure({}, {reco::timerange(50.0, 0.0, 100.0), reco::timerange(100.0, 90.0, 200.0)});
      expect_failure({}, {reco::timerange(150.0, 100.0, 200.0), reco::timerange(50.0, 0.0, 100.0)});
      expect_failure({}, {reco::timerange(100.0, 100.0, 100.0)});

      // Leave a two-window fixture for downstream tests, with reconstructed times outside their digit windows.
      run_case({late, early}, {reco::timerange(50.0, 0.0, 100.0), reco::timerange(150.0, 100.0, 200.0)});
      UFW_ASSERT(clusters.size() == 2, "Final two-window fixture lost a cluster");
      check_window(clusters[0], 0.0, 50.0, 100.0);
      check_window(clusters[1], 100.0, 150.0, 200.0);
      UFW_INFO("ECAL preclustering: passed {} focused cases", cases);
    }
  };

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::ecal_preclustering)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::ecal_preclustering)
