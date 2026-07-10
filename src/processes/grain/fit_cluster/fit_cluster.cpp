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

namespace sand::grain {

  /**
   * \class sand::grain::fit_cluster
   *
   * \brief 3D linear fit for clusterized points.
   *
   * Given a cluster (`point_clusters_in` input), this process refines the track estimation by performing weighted 3d linear fit
   * (`point_clusters_out` output) considering the amplitudes of the clusterized points.
   *
   * \subsection Configuration
   * | Parameter Name            | Type   | Unit  | Required/Default | Description                                                           |
   * |---------------------------|--------|-------|------------------|-----------------------------------------------------------------------|
   * | `fit_step_size`           | double |       | Default: 0.01    | Step size for line fit.                                               |
   * | `use_weights`             | bool   |       | Default: true    | Flag to use points amplitude as weights in fit minimizer.             |
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
    std::pair<pos_3d, dir_3d> weighted_linear_fit(const point_clusters::cluster& cluster);

   private:
    ROOT::Fit::Fitter m_fitter{};
    double m_fit_step_size;
    bool m_use_weights;
  };

  std::pair<pos_3d, dir_3d> fit_cluster::weighted_linear_fit(const point_clusters::cluster& cluster) {

    const pos_3d starting_line_point = cluster.centre();
    const dir_3d starting_line_dir = cluster.axis();
    UFW_DEBUG("Starting track: point {} direction {} n_points {}", starting_line_point, starting_line_dir, cluster.points().size());

    const std::array<double,4> starting_params = line_p_v_to_params(starting_line_point, starting_line_dir);

    WeightedLineFitter sdist(cluster.points(), m_use_weights);
    ROOT::Math::Functor fcn(sdist, starting_params.size());
    m_fitter.SetFCN(fcn, starting_params.data());

    for (int i{0}; i < starting_params.size(); ++i) {
      m_fitter.Config().ParSettings(i).SetStepSize(m_fit_step_size);
    }

    // Optional limits:
    m_fitter.Config().ParSettings(0).SetLimits(0.0, TMath::Pi()/2.0);
    // m_fitter.Config().ParSettings(1).SetLimits(0.0, 2.0*TMath::Pi());

    // If fit fails, return starting hough3d estimate as is
    if (!m_fitter.FitFCN()) {
      UFW_WARN("======== LINE FIT FAILED ======");
      return {starting_line_point, starting_line_dir};
    }

    const ROOT::Fit::FitResult& result = m_fitter.Result();

    std::array<double,4> fitted_params;
    std::array<double,4> errors;

    std::copy(result.Parameters().begin(), result.Parameters().end(), fitted_params.begin());
    std::copy(result.Errors().begin(), result.Errors().end(), errors.begin());

    return line_params_to_p_v(fitted_params);
  }

  void fit_cluster::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_use_weights = cfg.value("use_weights", true);
    m_fit_step_size = cfg.value("fit_step_size", 0.01);
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
        UFW_INFO("Skipping event with 0 clusters");
        continue;
      }
      UFW_DEBUG("Processing {} clusters", ev_clusters_in.size());
      std::vector<point_clusters::cluster> ev_clusters_out;
      for (const auto& clust : ev_clusters_in) {
        if (clust.points().size() == 0) {
          UFW_INFO("Skipping cluster with 0 points");
          continue;
        }

        auto [fitted_line_point, fitted_line_dir] = weighted_linear_fit(clust);
        UFW_DEBUG("Fitted track: point {} direction {}", fitted_line_point, fitted_line_dir);

        // Using placeholder time
        ev_clusters_out.emplace_back(fitted_line_point, fitted_line_dir, reco::timerange(0.0, 0.0), 0.0, clust.points());
      }
      point_clusters_out.push_back(ev_clusters_out);
    }

  }

} // namespace sand::grain

UFW_REGISTER_PROCESS(sand::grain::fit_cluster)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::fit_cluster)
