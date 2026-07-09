#include <common/sand.h>
#include <common/timerange.h>
#include <grain/point_cloud.h>
#include <grain/point_clusters.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <fit_cluster.hpp>

#include <Fit/Fitter.h>
#include <Math/Functor.h>

#include <cmath>
#include <vector>
#include <array>
#include <utility>
#include <algorithm>
#include <iostream>

namespace sand::grain {

  /**
   * \class sand::grain::fit_cluster
   *
   * \brief 3D linear fit for clusterized points.
   *
   * Given a cluster (`point_clusters_in` input), this process refines the track estimation by performing weighted 3d linear fit
   * (`point_clusters_in` output) considering the amplitudes of the clusterized points.
   *
   * \subsection Configuration
   * | Parameter Name            | Type   | Unit  | Required/Default | Description                                                           |
   * |---------------------------|--------|-------|------------------|-----------------------------------------------------------------------|
   *
   * \subsection Dependencies
   * | Type            | Comment  |
   * |-----------------|----------|
   * | `sand::geoinfo` | Geometry |
   *
   * \subsection Requirements
   * |  Name               | Type                          | Comment                           |
   * |---------------------|-------------------------------|-----------------------------------|
   * | `point_clusters_in` | `sand::grain::point_clusters` | Clusters found by Hough transform |
   *
   * \subsection Products
   * |  Name                | Type                          | Comment                           |
   * |----------------------|-------------------------------|-----------------------------------|
   * | `point_clusters_out` | `sand::grain::point_clusters` | Fitted clusters                   |
   */

  class fit_cluster : public ufw::process {
   public:
    fit_cluster();
    void configure(const ufw::config& cfg) override;
    void run() override;
    std::pair<pos_3d, dir_3d> weighted_linear_fit(const std::vector<point_cloud::point>& points);

   private:

  };

  std::pair<pos_3d, dir_3d> weighted_linear_fit(const std::vector<point_cloud::point>& points, const std::array<double,4>& starting_param, std::array<double,4>& fitted_param, std::array<double,4>& errors) {
      ROOT::Fit::Fitter fitter;

      WeightedLineFitter sdist(points);
      ROOT::Math::Functor fcn(sdist, starting_param.size());

      fitter.SetFCN(fcn, starting_param.data());

      for (int i{0}; i < starting_param.size(); ++i) {
          fitter.Config().ParSettings(i).SetStepSize(0.01);
      }

      // Optional limits:
      // fitter.Config().ParSettings(0).SetLimits(0.0, TMath::Pi()/2.0);
      // fitter.Config().ParSettings(1).SetLimits(0.0, 2.0*TMath::Pi());

      if (!fitter.FitFCN()) {
        UFW_WARN("======== LINE FIT FAILED ======");
          return std::pair<pos_3d, dir_3d>(pos_3d(std::nan(""), std::nan(""), std::nan("")), dir_3d(std::nan(""), std::nan(""), std::nan("")));
      }

      const ROOT::Fit::FitResult& result = fitter.Result();
      result.Print(std::cout);

      std::copy(result.Parameters().begin(), result.Parameters().end(), fitted_param.begin());
      std::copy(result.Errors().begin(), result.Errors().end(), errors.begin());

      return line_params_to_p_v(fitted_param);
  }

  void fit_cluster::configure(const ufw::config& cfg) {
    process::configure(cfg);
  }

  fit_cluster::fit_cluster() : process({{"point_clusters_in", "sand::grain::point_clusters"}},
                               {{"point_clusters_out", "sand::grain::point_clusters"}}) {
    UFW_DEBUG("Creating a fit_cluster process at {}.", fmt::ptr(this));
  }

  void fit_cluster::run() {
    UFW_DEBUG("Running a fit_cluster process at {}.", fmt::ptr(this));
    const auto& point_clusters_in = get<point_clusters>("point_clusters_in").clusters;
    auto& point_clusters_out = set<point_clusters>("point_clusters_out").clusters;
    // Loop on events in a spill
    for (const auto& ev_clusters_in : point_clusters_in) {
      if (ev_clusters_in.size() == 0) {
        UFW_DEBUG("Skipping event with 0 clusters");
        continue;
      }
      UFW_DEBUG("Processing {} clusters", ev_clusters_in.size());
      std::vector<point_clusters::cluster> ev_clusters_out;
      for (const auto& clust : ev_clusters_in) {
        // Using placeholder time
        const pos_3d out_line_point = clust.centre();
        const dir_3d out_line_dir = clust.axis();
        ev_clusters_out.emplace_back(out_line_point, out_line_dir, reco::timerange(0.0, 0.0), 0.0,
                                     clust.points());
        UFW_DEBUG("Added track: point {} direction {} n_points {}", out_line_point, out_line_dir,
                  clust.points().size());

      }
      point_clusters_out.push_back(ev_clusters_out);
    }

  }

} // namespace sand::grain

UFW_REGISTER_PROCESS(sand::grain::fit_cluster)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::fit_cluster)
