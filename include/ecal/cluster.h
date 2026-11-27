#pragma once

#include <array>
#include <cmath>
#include <vector>

#include <common/hit.h>
#include <ecal/reco_cell.h>

namespace sand::ecal {

  /// @brief ECAL cluster for preclustering, splitting, merging and fitting.
  ///
  /// reco::hit stores the energy-weighted centroid [mm], reconstructed time [ns],
  /// fitted unit direction and transverse RMS width [mm]. Uncomputed quantities are NaN.
  struct cluster : public reco::hit {
    enum class topology { unknown, barrel, endcap, mixed };

    enum class fit_status { not_run, insufficient_points, failed, success };

    cluster() : reco::hit(pos_3d(NAN, NAN, NAN), dir_3d(NAN, NAN, NAN), reco::timerange(), NAN) {}

    /// Owned reconstructed cells and digit pairs for subsequent splitting and merging.
    std::vector<reco_cell> cells;

    /// Source digit window [ns], with [earliest, latest) membership; distinct from t().
    /// Preserve it during splitting and keep clusters from different windows separate.
    reco::timerange digit_window;

    /// Total reconstructed deposited energy [MeV].
    double e = NAN;

    /// Full range of member reconstructed best times [ns], max(cell.time.best()) - min(cell.time.best()).
    /// Zero for a singleton.
    double time_span = NAN;

    /// Energy-weighted spatial standard deviations [mm] in x, y, z, with Neff/(Neff-1) correction.
    /// Member spread, not centroid uncertainty; zero for a singleton.
    std::array<double, 3> sigma = {NAN, NAN, NAN};

    /// Fitted cluster starting point [mm].
    pos_3d apex = {NAN, NAN, NAN};

    /// Detector topology; preclustering never produces mixed clusters.
    topology type = topology::unknown;

    /// Direction and apex fit status; reset to not_run when cells change.
    fit_status fit = fit_status::not_run;
  };

} // namespace sand::ecal

SAND_DATA_COLLECTION(sand::ecal, cluster, clusters)
