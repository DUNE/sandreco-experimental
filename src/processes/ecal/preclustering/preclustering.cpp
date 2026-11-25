#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include <common/timeslicer.h>
#include <ecal/cluster.h>
#include <geoinfo/ecal_info.hpp>
#include <geoinfo/geoinfo.hpp>

#include <ufw/factory.hpp>

#include "preclustering.hpp"

namespace sand::ecal {

  namespace {

    using cell_id = geoinfo::ecal_info::cell_id;

    // Non-owning reco_cell entry with a cached physical cell ID for sorting and traversal.
    struct cell_ref {
      const reco_cell* cell;
      cell_id id;

      // Use the begin-digit TDC for membership; fibre-corrected cell.time may lie outside the digit window.
      double t() const { return cell->begin.tdc(); }
    };

    // Require ordered, nonempty, disjoint [earliest, latest) windows.
    // Bounds may be infinite; best must be finite with earliest <= best <= latest.
    void validate_windows(const reco::timeranges& windows) {
      for (std::size_t i = 0; i < windows.size(); ++i) {
        const auto& window = windows[i];
        if (std::isnan(window.earliest()) || std::isnan(window.latest()) || !std::isfinite(window.best())
            || window.earliest() >= window.latest() || window.earliest() > window.best()
            || window.best() > window.latest() || (i != 0 && windows[i - 1].latest() > window.earliest())) {
          UFW_ERROR("ECAL preclustering: digit windows must be ordered, disjoint, nonempty intervals with a valid "
                    "representative time (window {})",
                    i);
        }
      }
    }

    cell_id validate_cell(const reco_cell& cell, const geoinfo::ecal_info& ecal) {
      using face       = geoinfo::ecal_info::face_location;
      const auto begin = ecal.pmt(cell.begin.channel());
      const auto end   = ecal.pmt(cell.end.channel());
      // Require begin/end readout faces of the same known ECAL cell.
      if (cell.begin.channel().subdetector != subdetector_t::ECAL
          || cell.end.channel().subdetector != subdetector_t::ECAL || begin.face_ != face::begin
          || end.face_ != face::end || begin.cell_.raw != end.cell_.raw || !ecal.contains(begin.cell_)) {
        UFW_ERROR("ECAL preclustering: invalid complete digit pair for cell {}", begin.cell_.raw);
      }
      const auto& time = cell.time;
      // Reject nonfinite observables before time sorting and weighted accumulation.
      if (!std::isfinite(cell.e) || cell.e < 0.0 || !std::isfinite(cell.position.x())
          || !std::isfinite(cell.position.y()) || !std::isfinite(cell.position.z()) || !std::isfinite(time.best())
          || !std::isfinite(time.earliest()) || !std::isfinite(time.latest()) || time.earliest() > time.best()
          || time.best() > time.latest() || !std::isfinite(cell.begin.tdc()) || !std::isfinite(cell.end.tdc())) {
        UFW_ERROR("ECAL preclustering: invalid reconstructed quantities for cell {}", begin.cell_.raw);
      }
      return begin.cell_;
    }

    // Energy-weighted precluster observables, with E_i > 0 and E = sum_i E_i:
    //   r_cl = sum_i E_i r_i / E
    //   t_cl,k = sum_i E_i t_i,k / E, k in {best, earliest, latest}
    //   time_span = max_i t_i,best - min_i t_i,best
    //   S_q = sum_i E_i (q_i - q_cl)^2, q in {x, y, z}
    //   sigma_q^2 = S_q E / (E^2 - sum_i E_i^2); singleton sigma_q = 0.
    // The variance includes the Neff/(Neff-1) correction, Neff = E^2 / sum_i E_i^2.
    cluster calculate_precluster_quantities(std::vector<reco_cell>&& member_cells, geo_id::region_t region,
                                            const reco::timerange& digit_window) {
      cluster result;
      result.cells = std::move(member_cells);
      result.type  = region == geo_id::BARREL ? cluster::topology::barrel : cluster::topology::endcap;
      // Preserve digit-window provenance independently of fibre-corrected cluster time.
      result.digit_window = digit_window;

      long double total_energy                = 0.0L;
      long double sum_of_cell_energy_products = 0.0L;
      long double best_time_ns                = 0.0L;
      long double earliest_time_ns            = 0.0L;
      long double latest_time_ns              = 0.0L;
      long double minimum_time_ns             = result.cells.front().time.best();
      long double maximum_time_ns             = minimum_time_ns;
      // Axis order: x, y, z. Weighted Welford updates avoid cancellation in S_q.
      std::array<long double, 3> mean_position               = {};
      std::array<long double, 3> weighted_squared_deviations = {};
      for (const auto& cell : result.cells) {
        const long double cell_energy           = cell.e;
        const long double previous_total_energy = total_energy;
        // Accumulate sum_{i<j} E_i E_j using the preceding total energy.
        sum_of_cell_energy_products += previous_total_energy * cell_energy;
        total_energy += cell_energy;
        // Running weight E_i / E_running, including the current cell.
        const long double cell_energy_fraction         = cell_energy / total_energy;
        const std::array<long double, 3> cell_position = {cell.position.x(), cell.position.y(), cell.position.z()};
        for (std::size_t axis = 0; axis < mean_position.size(); ++axis) {
          // Use the previous centroid's displacement in both Welford recurrences.
          const long double position_difference = cell_position[axis] - mean_position[axis];
          mean_position[axis] += cell_energy_fraction * position_difference;
          weighted_squared_deviations[axis] +=
              previous_total_energy * cell_energy_fraction * position_difference * position_difference;
        }
        minimum_time_ns = std::min(minimum_time_ns, static_cast<long double>(cell.time.best()));
        maximum_time_ns = std::max(maximum_time_ns, static_cast<long double>(cell.time.best()));
        // Average best, earliest and latest times independently with the same energy weights.
        best_time_ns += cell_energy_fraction * (cell.time.best() - best_time_ns);
        earliest_time_ns += cell_energy_fraction * (cell.time.earliest() - earliest_time_ns);
        latest_time_ns += cell_energy_fraction * (cell.time.latest() - latest_time_ns);
      }

      result.e         = static_cast<double>(total_energy);
      result.time_span = static_cast<double>(maximum_time_ns - minimum_time_ns);
      if (!std::isfinite(result.e) || !std::isfinite(static_cast<double>(best_time_ns))
          || !std::isfinite(result.time_span) || !std::isfinite(static_cast<double>(earliest_time_ns))
          || !std::isfinite(static_cast<double>(latest_time_ns))) {
        UFW_ERROR("ECAL preclustering: cluster properties exceed the finite numeric range");
      }

      // D = 2 * sum_{i<j} E_i E_j = E^2 - sum_i E_i^2, evaluated without subtraction.
      // Normalize as sigma_q^2 = S_q E / D; use zero variance when D = 0 (singleton).
      const long double variance_denominator = 2.0L * sum_of_cell_energy_products;
      for (std::size_t axis = 0; axis < mean_position.size(); ++axis) {
        const long double position_variance =
            variance_denominator > 0.0L
                ? std::max(0.0L, weighted_squared_deviations[axis]) * total_energy / variance_denominator
                : 0.0L;
        result.sigma[axis] = static_cast<double>(std::sqrt(position_variance));
        if (!std::isfinite(static_cast<double>(mean_position[axis])) || !std::isfinite(result.sigma[axis])) {
          UFW_ERROR("ECAL preclustering: cluster properties exceed the finite numeric range");
        }
      }
      // Set centroid and time in the cluster's reco::hit base. Direction, width and apex
      // remain NaN, with fit_status::not_run until fitting.
      static_cast<reco::hit&>(result) =
          reco::hit(pos_3d(mean_position[0], mean_position[1], mean_position[2]), dir_3d(NAN, NAN, NAN),
                    reco::timerange(best_time_ns, earliest_time_ns, latest_time_ns), NAN);
      return result;
    }

  } // namespace

  preclustering::preclustering()
    : process({{"reco_cells", "sand::ecal::reco_cells"}, {"timeranges", "sand::reco::timeranges"}},
              {{"clusters", "sand::ecal::clusters"}}) {}

  void preclustering::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_print_output = cfg.value("print_output", m_print_output);
  }

  void preclustering::run() {
    const auto& ecal    = instance<geoinfo>().ecal();
    const auto& input   = get<reco_cells>("reco_cells");
    const auto& windows = get<reco::timeranges>("timeranges");
    auto& output        = set<clusters>("clusters");
    output.clear();

    // Select complete, unambiguous reco_cells with E > 0; exclude originally incomplete pairs.
    validate_windows(windows);
    std::vector<cell_ref> ordered;
    ordered.reserve(input.size());
    for (const auto& cell : input) {
      if (!cell.unambiguous_complete() || cell.originally_incomplete || cell.e == 0.0) {
        continue;
      }
      ordered.push_back({&cell, validate_cell(cell, ecal)});
    }
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const auto& lhs, const auto& rhs) { return lhs.t() < rhs.t(); });
    const reco::timeslicer slicer(windows);
    // Scan disjoint windows once; omit windows without selected cells and cells in gaps.
    const auto views = slicer.slice_disjoint(ordered.cbegin(), ordered.cend());

    // Cluster each digit window independently.
    for (const auto& view : views) {
      // Group all accepted reco_cell references by raw physical cell ID.
      // Ascending raw IDs fix precluster seed order without cell_id comparisons.
      std::map<std::uint32_t, std::vector<cell_ref>> unassigned_cells;
      for (const auto& reco_cell_reference : view) {
        // begin.tdc() determined view membership; require end.tdc() in the same window.
        if (reco_cell_reference.cell->end.tdc() < view.earliest()
            || reco_cell_reference.cell->end.tdc() >= view.latest()) {
          UFW_ERROR("ECAL preclustering: both digits of cell {} must belong to the same digit window",
                    reco_cell_reference.id.raw);
        }
        unassigned_cells[reco_cell_reference.id.raw].push_back(reco_cell_reference);
      }
      UFW_DEBUG("ECAL preclustering: digit window [{}, {}) ns has {} eligible reconstructed cells in {} physical cells",
                view.earliest(), view.latest(), view.size(), unassigned_cells.size());

      // Build preclusters by breadth-first traversal of ecal.neighbours(), restricted
      // to physical cell IDs with accepted reco_cells. No additional position or
      // pairwise time cuts are applied.
      while (!unassigned_cells.empty()) {
        const auto seed                          = unassigned_cells.begin()->second.front().id;
        std::vector<cell_id> cell_ids_to_explore = {seed};
        std::vector<reco_cell> members;

        // Copy all reco_cells for this ID, then erase it to mark the ID assigned.
        // Erasing on discovery prevents duplicate assignments and revisits through cycles.
        auto append = [&](auto entry) {
          for (const auto& reco_cell_reference : entry->second) {
            members.push_back(*reco_cell_reference.cell);
          }
          unassigned_cells.erase(entry);
        };
        // Assign the seed before expanding its neighbours, including isolated seeds.
        append(unassigned_cells.begin());
        // The queue grows during traversal; keep the loop bound tied to its current size.
        for (std::size_t i = 0; i < cell_ids_to_explore.size(); ++i) {
          for (const auto neighbour : ecal.neighbours(cell_ids_to_explore[i])) {
            // Enforce region separation: barrel, endcap A and endcap B.
            if (neighbour.region != seed.region) {
              continue;
            }
            // Only neighbours with unassigned reco_cells can extend the precluster.
            const auto found = unassigned_cells.find(neighbour.raw);
            if (found != unassigned_cells.end()) {
              cell_ids_to_explore.push_back(neighbour);
              append(found);
            }
          }
        }
        // Compute precluster observables and retain the source digit window after traversal.
        output.push_back(calculate_precluster_quantities(std::move(members), seed.region, view));
      }
    }

    UFW_DEBUG("ECAL preclustering: {} reconstructed cells, {} digit windows, {} clusters", input.size(), windows.size(),
              output.size());
    if (m_print_output) {
      for (std::size_t i = 0; i < output.size(); ++i) {
        const auto& cluster = output[i];
        UFW_INFO("  precluster {}: digit window=[{}, {}) ns, {} cells, E={} MeV, centre={} mm, time={} ns, "
                 "time span={} ns, sigma=({}, {}, {}) mm",
                 i, cluster.digit_window.earliest(), cluster.digit_window.latest(), cluster.cells.size(), cluster.e,
                 cluster.centre(), cluster.t(), cluster.time_span, cluster.sigma[0], cluster.sigma[1],
                 cluster.sigma[2]);
        for (const auto& cell : cluster.cells) {
          UFW_INFO("    cell {}", ecal.pmt(cell.begin.channel()).cell_.raw);
        }
      }
    }
  }

} // namespace sand::ecal

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::ecal::preclustering)
