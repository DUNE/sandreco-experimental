#include <drift_fast_digi.hpp>

#include <edep_reader/edep_reader.hpp>
#include <geoinfo/drift_info.hpp>
#include <geoinfo/geoinfo.hpp>
#include <geoinfo/tracker_info.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>
#include <tracker/digi.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <algorithm>
#include <limits>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>

namespace sand::drift {

  /**
   * \class sand::drift::drift_fast_digi
   *
   * \brief Fast digitization of drift-chamber energy deposits into wire signals.
   *
   * Drift-chamber fast-digitization algorithm: assignment of the
   * DRIFT energy deposits to readout views and wires, splitting of hits that
   * cross several wires into per-wire sub-hits, hit-to-wire closest-approach
   * geometry, drift and in-wire propagation timing, and construction of the
   * digitized signals.
   * Assigns the DRIFT energy deposits of the event to readout views and wires — splitting
   * a hit that crosses a cell boundary into one sub-hit per wire, with the energy shared by
   * segment length — and turns the hits collected on each fired wire into one or more digis.
   * The hit arrival times at the readout end (drift plus in-wire propagation) are sorted; a
   * digi opens at the first arrival and collects every hit arriving within `integration_time`;
   * hits arriving in the following `dead_time` are discarded and the next arrival opens a new
   * digi on the same wire. Every hit collected in a digi becomes one of its signals, storing
   * the raw arrival time, the deposited energy and the hit id for Monte Carlo truth matching.
   * The digi TDC is the earliest signal time Gaussian-smeared by `sigma_tdc`; the digi ADC is
   * the sum of the signal energies.
   *
   * \subsection Configuration
   * | Parameter Name   | Type     | Unit  | Required/Default | Description                          |
   * |------------------|----------|-------|------------------|--------------------------------------|
   * | `drift_velocity` | `double` | mm/ns | Required         | Drift velocity of ionized particles. |
   * | `wire_velocity`  | `double` | mm/ns | Required         | Signal transit speed along the wire. |
   * | `sigma_tdc`      | `double` | ns    | Required         | Gaussian time resolution of the TDC. |
   * | `integration_time` | `double` | ns  | Required         | Signal integration window.           |
   * | `dead_time`      | `double` | ns    | Required         | Dead time after the integration window. |
   *
   * \subsection Dependencies
   * | Type                     | Comment                                |
   * |--------------------------|----------------------------------------|
   * | `sand::geoinfo`          | Geometry                               |
   * | `sand::edep_reader`      | Simulated energy deposits of the event |
   * | `sand::root_tgeomanager` | ROOT geometry navigator                |
   *
   * \subsection Requirements
   * None; the inputs are unique objects fetched by type (see Dependencies).
   *
   * \subsection Products
   * |  Name   | Type                            | Comment                                  |
   * |---------|---------------------------------|------------------------------------------|
   * | `digi`  | `sand::tracker::digi_container` | One digi per integration window per fired wire, one signal per contributing hit |
   */
  /**
   * \brief Reads and stores the configuration parameters.
   * \param cfg The JSON configuration fragment for this process.
   */

  void drift_fast_digi::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_drift_velocity   = cfg.at("drift_velocity");
    m_wire_velocity    = cfg.at("wire_velocity");
    m_sigma_tdc        = cfg.at("sigma_tdc");
    m_integration_time = cfg.at("integration_time");
    m_dead_time        = cfg.at("dead_time");
  }

  /// \brief Declares the process I/O: no requirements and a single product \c digi.
  drift_fast_digi::drift_fast_digi() : process({}, {{"digi", "sand::tracker::digi_container"}}) {
    UFW_DEBUG("Creating a drift_fast_digi process at {}", fmt::ptr(this));
  }

  /**
   * \brief Digitizes the current event.
   *
   * Groups the DRIFT hits per wire and fills the \c digi product with one
   * digi per integration window on each fired wire.
   */
  void drift_fast_digi::run() {
    UFW_DEBUG("Running drift_fast_digi process at {}", fmt::ptr(this));
    // Two phases: first assign every DRIFT hit (splitting it across wires when
    // needed) to its wire, then turn the hits collected on each wire into one
    // digi per integration window, holding one signal per contributing hit.
    std::map<const geoinfo::tracker_info::wire*, std::vector<EDEPHit>> hits_by_wire = group_hits_by_wire();
    digitize_hits_in_wires(hits_by_wire);
  }

  /**
   * \brief Groups the DRIFT hits of the current event by the wire they belong to.
   *
   * For each hit the start and stop points are mapped to a readout view; hits
   * on an invalid plane or spanning two views are skipped. Within the view the
   * closest wire to each end is found and the hit is either assigned to a single
   * wire or split across the wires it crosses with \ref split_hit.
   *
   * \return A map from wire to the hits (whole or split) collected on that wire.
   */
  std::map<const geoinfo::tracker_info::wire*, std::vector<EDEPHit>> drift_fast_digi::group_hits_by_wire() {
    const auto& gi   = instance<geoinfo>();
    const auto& tree = instance<sand::edep_reader>();
    auto& tgm        = instance<root_tgeomanager>();
    std::map<const geoinfo::tracker_info::wire*, std::vector<EDEPHit>> hits_by_wire;
    const auto* drift = dynamic_cast<const sand::geoinfo::drift_info*>(&gi.tracker());

    for (const auto& trj : tree) {
      const auto& hit_map = trj.GetHitMap(); // pointer, not value
      if (!trj.HasHitInDetector(sand::subdetector_t::DRIFT))
        continue;

      UFW_INFO("Found {} DRIFT hits for trajectory with ID {}", hit_map.at(sand::subdetector_t::DRIFT).size(),
               trj.GetId());

      for (const auto& hit : hit_map.at(sand::subdetector_t::DRIFT)) {
        sand::pos_3d hit_start_3d(hit.GetStart().X(), hit.GetStart().Y(), hit.GetStart().Z());
        auto direction = hit.GetStop() - hit.GetStart();
        sand::dir_3d direction_3d(direction.X(), direction.Y(), direction.Z());
        tgm.navigator()->set_track(hit_start_3d, direction_3d);

        tgm.navigator()->FindNode(hit.GetStart().X(), hit.GetStart().Y(), hit.GetStart().Z());
        tgm.navigator()->FindNextBoundary(1000);
        if (tgm.navigator()->GetStep() < 1E-5) {
          tgm.navigator()->Step();
          tgm.navigator()->GetCurrentNode();
        }

        geo_path start_path(tgm.navigator()->GetPath());
        geo_path start_partial_path = drift->partial_path(start_path, gi);
        UFW_DEBUG("start_partial_path: {}", start_partial_path);
        geo_id start_ID = drift->id(start_partial_path);

        sand::pos_3d hit_stop_3d(hit.GetStop().X(), hit.GetStop().Y(), hit.GetStop().Z());
        tgm.navigator()->set_track(hit_stop_3d, -direction_3d);
        tgm.navigator()->FindNode(hit.GetStop().X(), hit.GetStop().Y(), hit.GetStop().Z());
        tgm.navigator()->FindNextBoundary(1000);
        if (tgm.navigator()->GetStep() < 1E-5) {
          tgm.navigator()->Step();
          tgm.navigator()->GetCurrentNode();
        }
        geo_path stop_path(tgm.navigator()->GetPath());
        geo_path stop_partial_path = drift->partial_path(stop_path, gi);
        UFW_DEBUG("stop_partial_path: {}", stop_partial_path);
        geo_id stop_ID = drift->id(stop_partial_path);

        if (start_ID.drift.plane == 255 || stop_ID.drift.plane == 255) {
          UFW_DEBUG(" One of two hit_segment ends has invalid plane ID, skipping.");
          continue;
        }
        if (start_ID.raw != stop_ID.raw) {
          UFW_DEBUG(" Hit spans multiple views: start view ID ({},{},{}), stop view ID ({},{},{}). Skipping.",
                    start_ID.drift.subdetector, start_ID.drift.supermodule, start_ID.drift.plane,
                    stop_ID.drift.subdetector, stop_ID.drift.supermodule, stop_ID.drift.plane);
          continue;
        }

        const auto* drift_station = static_cast<const sand::geoinfo::drift_info::station*>(
            drift->get_station_by_ID(start_ID.drift.supermodule));
        // Pick the wire list of the view the hit belongs to: plane 0 -> X, 1 -> U, 2 -> V.
        geoinfo::tracker_info::wire_list wires_in_view;
        if (start_ID.drift.plane == 0) {
          wires_in_view = drift_station->x_view();
        } else if (start_ID.drift.plane == 1) {
          wires_in_view = drift_station->u_view();
        } else if (start_ID.drift.plane == 2) {
          wires_in_view = drift_station->v_view();
        }

        const auto [closest_wire_start, closest_wire_start_index] =
            drift->closest_wire_in_list(wires_in_view, pos_3d(hit.GetStart().Vect()));
        const auto [closest_wire_stop, closest_wire_stop_index] =
            drift->closest_wire_in_list(wires_in_view, pos_3d(hit.GetStop().Vect()));

        if (closest_wire_start_index == SIZE_MAX || closest_wire_stop_index == SIZE_MAX) {
          UFW_DEBUG(" Could not find closest wire index for one end of the hit, skipping.");
          continue;
        }
        if (closest_wire_start == nullptr || closest_wire_stop == nullptr) {
          UFW_DEBUG(" Could not find closest wire for one end of the hit, skipping.");
          continue;
        }

        if (closest_wire_start == closest_wire_stop) {
          // Both ends fall on the same wire: keep the hit whole.
          UFW_DEBUG(" For hit ID ({}) closest wire start and stop are the same wire: wire ID: ({}).", hit.GetId(),
                    closest_wire_start_index);
          hits_by_wire[closest_wire_start].emplace_back(hit);
        } else {
          // The hit crosses several wires: split it into one sub-hit per wire.
          UFW_DEBUG(" For hit ID ({}) closest wire start and stop are different wires: wire ID: ({},{}).", hit.GetId(),
                    closest_wire_start_index, closest_wire_stop_index);
          auto split_hits = split_hit(closest_wire_start_index, closest_wire_stop_index, wires_in_view, hit);
          UFW_DEBUG(" Split hit between {} wires.", split_hits.size());
          for (const auto& [wire, split_hit] : split_hits) {
            UFW_DEBUG(" Hit energy {}", split_hit.GetEnergyDeposit());
            hits_by_wire[wire].emplace_back(split_hit);
          }
        }
      }
    }

    UFW_INFO("Hits have been grouped associated to {} wires.", hits_by_wire.size());
    return hits_by_wire;
  }

  /**
   * \brief Transverse coordinate at which the current segment ends during hit splitting.
   *
   * The boundary is the midpoint between the centers of \p current_wire and
   * \p next_wire (in local-frame transverse coordinate). If that boundary lies
   * beyond the hit end, the hit end is returned instead; if \p next_wire is null
   * the hit end is returned.
   *
   * \param current_wire         The wire the current segment belongs to.
   * \param next_wire            The following wire, or \c nullptr if \p current_wire is the last.
   * \param wire_plane_transform Transform from local wire-plane coordinates to global coordinates.
   * \param transverse_start     Transverse coordinate of the current segment start.
   * \param transverse_end       Transverse coordinate of the hit end.
   * \param wire_index           Index of \p current_wire (used for logging).
   * \return The transverse coordinate of the segment end.
   */
  double drift_fast_digi::calculate_wire_boundary_transverse(const geoinfo::tracker_info::wire* current_wire,
                                                             const geoinfo::tracker_info::wire* next_wire,
                                                             const xform_3d& wire_plane_transform,
                                                             double transverse_start, double transverse_end,
                                                             size_t wire_index) const {
    if (next_wire == nullptr) {
      return transverse_end;
    }

    // Calculate wire centers in local coordinates
    const pos_3d wire1_center_global = (current_wire->head + dir_3d(current_wire->tail)) * 0.5;
    const pos_3d wire1_center_local  = wire_plane_transform.Inverse() * wire1_center_global;
    const double wire1_transverse    = wire1_center_local.Y();

    const pos_3d wire2_center_global = (next_wire->head + dir_3d(next_wire->tail)) * 0.5;
    const pos_3d wire2_center_local  = wire_plane_transform.Inverse() * wire2_center_global;
    const double wire2_transverse    = wire2_center_local.Y();

    // Midpoint between adjacent wires defines the boundary
    const double boundary_transverse = 0.5 * (wire1_transverse + wire2_transverse);

    UFW_DEBUG(" Wire1 index: {}, transverse coord: {}", wire_index, wire1_transverse);
    UFW_DEBUG(" Wire2 index: {}, transverse coord: {}", wire_index + 1, wire2_transverse);
    UFW_DEBUG(" Boundary transverse coord: {}", boundary_transverse);

    // Check if boundary is between start and end, otherwise use end
    const double distance_to_boundary = fabs(boundary_transverse - transverse_start);
    const double distance_to_end      = fabs(transverse_end - transverse_start);

    return (distance_to_boundary < distance_to_end) ? boundary_transverse : transverse_end;
  }

  /**
   * \brief Emits debug-only logging for a single hit segment.
   * \param segment_start_global Segment start in global coordinates.
   * \param segment_end_global   Segment end in global coordinates.
   * \param segment_end_local    Segment end in local wire-plane coordinates.
   * \param segment_length       Length of the segment.
   * \param segment_fraction     Fraction of the total hit length carried by this segment.
   * \param wire_index           Index of the wire the segment belongs to.
   */
  void drift_fast_digi::log_segment_debug(const pos_3d& segment_start_global, const pos_3d& segment_end_global,
                                          const pos_3d& segment_end_local, double segment_length,
                                          double segment_fraction, size_t wire_index) const {
    UFW_DEBUG(" ===== Segment {} Debug Info =====", wire_index);
    UFW_DEBUG("   Start global: {}", segment_start_global);
    UFW_DEBUG("   End global:   {}", segment_end_global);
    UFW_DEBUG("   End local:    {}", segment_end_local);
    UFW_DEBUG("   Length: {:.3f}, Fraction: {:.4f}", segment_length, segment_fraction);
  }

  /**
   * \brief Interpolates the hit segment to the point at a given transverse coordinate.
   *
   * Linear interpolation is performed along the hit direction using the local
   * coordinate deltas, then the endpoint is transformed back to global coordinates.
   *
   * \param start_local            Segment start in local wire-plane coordinates.
   * \param dx_local               Local x-sand::subdetector_t of the hit direction.
   * \param dy_local               Local y-sand::subdetector_t (transverse) of the hit direction.
   * \param dz_local               Local z-sand::subdetector_t of the hit direction.
   * \param segment_end_transverse Transverse coordinate at which the segment ends.
   * \param transform              Transform from local wire-plane coordinates to global coordinates.
   * \return A pair {endpoint in local coordinates, endpoint in global coordinates}.
   */
  std::pair<pos_3d, pos_3d> drift_fast_digi::interpolate_segment_endpoint(const pos_3d& start_local, double dx_local,
                                                                          double dy_local, double dz_local,
                                                                          double segment_end_transverse,
                                                                          const xform_3d& transform) const {
    // Linear interpolation parameter: fraction along hit segment
    const double interpolation_param = fabs((segment_end_transverse - start_local.Y()) / dy_local);

    // Calculate segment endpoint in local coordinates
    const pos_3d segment_end_local(start_local.X() + dx_local * interpolation_param,
                                   start_local.Y() + dy_local * interpolation_param,
                                   start_local.Z() + dz_local * interpolation_param);

    // Transform to global coordinates
    const pos_3d segment_end_global = transform * segment_end_local;

    return {segment_end_local, segment_end_global};
  }

  /**
   * \brief Builds the sub-hit corresponding to one wire segment.
   *
   * The energy, secondary deposit and track length of \p original_hit are scaled
   * by \p segment_fraction; the segment end time is derived from the start time and
   * the fraction of the total time span; contributor, primary and hit identifiers
   * are inherited from \p original_hit.
   *
   * \param segment_start_global Segment start in global coordinates.
   * \param segment_end_global   Segment end in global coordinates.
   * \param segment_start_time   Time at the segment start [ns].
   * \param time_direction       +1 or -1, the sign of time progression along the traversal.
   * \param total_time_span      Total time span of the original hit [ns].
   * \param segment_fraction     Fraction of the total hit length carried by this segment.
   * \param original_hit         The hit being split.
   * \return The sub-hit for this segment.
   */
  EDEPHit drift_fast_digi::create_segment_hit(const pos_3d& segment_start_global, const pos_3d& segment_end_global,
                                              double segment_start_time, double time_direction, double total_time_span,
                                              double segment_fraction, const EDEPHit& original_hit) const {
    const vec_4d segment_start_4d(segment_start_global.X(), segment_start_global.Y(), segment_start_global.Z(),
                                  segment_start_time);
    const vec_4d segment_end_4d(segment_end_global.X(), segment_end_global.Y(), segment_end_global.Z(),
                                segment_start_time + time_direction * total_time_span * segment_fraction);

    const double scaled_energy       = original_hit.GetEnergyDeposit() * segment_fraction;
    const double scaled_secondary    = original_hit.GetSecondaryDeposit() * segment_fraction;
    const double scaled_track_length = original_hit.GetTrackLength() * segment_fraction;

    return EDEPHit(segment_start_4d, segment_end_4d, scaled_energy, scaled_secondary, scaled_track_length,
                   original_hit.GetContrib(), // TO-DO: How to split contributor?
                   original_hit.GetPrimaryId(), original_hit.GetId());
  }

  /**
   * \brief Splits a hit that crosses several wires of a view into one sub-hit per wire.
   *
   * The computation is carried out in the wire-plane local frame. The wires are
   * walked from the first to the last crossed one (reversing the traversal if the
   * start index is larger than the stop index); at each inter-wire boundary the
   * segment endpoint is interpolated and the deposited energy, secondary energy
   * and track length are scaled by the segment length fraction.
   *
   * \param closest_wire_start_index Index, within \p wires_in_view, of the wire closest to the hit start.
   * \param closest_wire_stop_index  Index, within \p wires_in_view, of the wire closest to the hit stop.
   * \param wires_in_view            The ordered list of wires of the view the hit belongs to.
   * \param hit                      The hit to be split.
   * \return A map from wire to the sub-hit assigned to it.
   */
  std::map<const geoinfo::tracker_info::wire*, EDEPHit>
  drift_fast_digi::split_hit(size_t closest_wire_start_index, size_t closest_wire_stop_index,
                             const geoinfo::tracker_info::wire_list& wires_in_view, const EDEPHit& hit) {
    const auto& gi    = instance<geoinfo>();
    const auto* drift = dynamic_cast<const sand::geoinfo::drift_info*>(&gi.tracker());
    std::map<const geoinfo::tracker_info::wire*, EDEPHit> split_hit;

    // Strategy: work in the wire-plane local frame, walk the crossed wires from
    // first to last, and at each inter-wire boundary cut a segment whose
    // deposited quantities are scaled by its length fraction of the whole hit.

    // Extract hit segment properties
    const double total_hit_length = sqrt((hit.GetStop().Vect() - hit.GetStart().Vect()).Mag2());
    const double total_time_span  = (hit.GetStop() - hit.GetStart()).T();
    const double hit_start_time   = hit.GetStart().T();

    // Get coordinate transformation for wire plane
    const xform_3d wire_plane_transform = wires_in_view.at(closest_wire_start_index)->wire_plane_transform();

    // Transform hit endpoints from global to local rotated coordinates
    const pos_3d hit_start_global = pos_3d(hit.GetStart().X(), hit.GetStart().Y(), hit.GetStart().Z());
    const pos_3d hit_start_local  = wire_plane_transform.Inverse() * hit_start_global;

    const pos_3d hit_stop_global = pos_3d(hit.GetStop().X(), hit.GetStop().Y(), hit.GetStop().Z());
    const pos_3d hit_stop_local  = wire_plane_transform.Inverse() * hit_stop_global;

    UFW_DEBUG(" Total hit key properties: Start {}, Stop {}, EnergyDeposit: {}, SecondaryDeposit: {}, TrackLength: {}, "
              "Contrib: {}, PrimaryId: {}, Id: {}",
              vec_4d(hit.GetStart()), vec_4d(hit.GetStop()), hit.GetEnergyDeposit(), hit.GetSecondaryDeposit(),
              hit.GetTrackLength(), hit.GetContrib(), hit.GetPrimaryId(), hit.GetId());

    // Setup iteration parameters: ensure we iterate from first to last wire
    const bool need_to_reverse = (closest_wire_start_index > closest_wire_stop_index);

    size_t first_wire_index = need_to_reverse ? closest_wire_stop_index : closest_wire_start_index;
    size_t last_wire_index  = need_to_reverse ? closest_wire_start_index : closest_wire_stop_index;

    // Starting point for iteration (may be swapped if reversing)
    pos_3d segment_start_global = need_to_reverse ? hit_stop_global : hit_start_global;
    pos_3d segment_start_local  = need_to_reverse ? hit_stop_local : hit_start_local;
    double segment_start_time   = need_to_reverse ? (hit_start_time + total_time_span) : hit_start_time;
    double time_direction       = need_to_reverse ? -1.0 : 1.0;

    // Transverse coordinates (Y in local frame) for interpolation
    const double transverse_start = segment_start_local.Y();
    const double transverse_end   = need_to_reverse ? hit_start_local.Y() : hit_stop_local.Y();

    // Local coordinate deltas for linear interpolation
    const double dx_local = (need_to_reverse ? -1.0 : 1.0) * (hit_stop_local.X() - hit_start_local.X());
    const double dy_local = (need_to_reverse ? -1.0 : 1.0) * (hit_stop_local.Y() - hit_start_local.Y());
    const double dz_local = (need_to_reverse ? -1.0 : 1.0) * (hit_stop_local.Z() - hit_start_local.Z());

    if (need_to_reverse) {
      UFW_DEBUG(" Swapping closest wire start and stop to maintain order.");
    }

    // Iterate through wires, splitting hit into segments
    double cumulative_energy           = 0;
    double cumulative_secondary_energy = 0;
    double cumulative_length           = 0;
    vec_4d last_hit_stop;
    for (size_t wire_index = first_wire_index; wire_index <= last_wire_index; ++wire_index) {
      const auto* current_wire = wires_in_view.at(wire_index);
      const bool is_last_wire  = (wire_index + 1 >= wires_in_view.size());
      const auto* next_wire    = is_last_wire ? nullptr : wires_in_view.at(wire_index + 1);

      if (next_wire == nullptr) {
        UFW_DEBUG(" Reached last wire in view during hit splitting.");
        split_hit[current_wire] =
            EDEPHit(last_hit_stop, hit.GetStop(), hit.GetEnergyDeposit() - cumulative_energy,
                    hit.GetSecondaryDeposit() - cumulative_secondary_energy, hit.GetTrackLength() - cumulative_length,
                    hit.GetContrib(), // TO-DO: How to split contributor?
                    hit.GetPrimaryId(), hit.GetId());
        break;
      }

      // Determine the transverse coordinate where this segment ends
      const double segment_end_transverse = calculate_wire_boundary_transverse(
          current_wire, next_wire, wire_plane_transform, segment_start_local.Y(), transverse_end, wire_index);

      // Calculate segment endpoint in both local and global coordinates
      const auto [segment_end_local, segment_end_global] = interpolate_segment_endpoint(
          segment_start_local, dx_local, dy_local, dz_local, segment_end_transverse, wire_plane_transform);

      // Calculate what fraction of the total hit this segment represents
      const double segment_length   = sqrt((segment_end_global - segment_start_global).Mag2());
      const double segment_fraction = segment_length / total_hit_length;

      // Validate segment fraction is reasonable
      if (segment_fraction < 0.0 || segment_fraction > 1.0) {
        UFW_WARN(" Segment fraction out of range [0,1]: {} for wire index {}", segment_fraction, wire_index);
      }

      log_segment_debug(segment_start_global, segment_end_global, segment_end_local, segment_length, segment_fraction,
                        wire_index);

      // Build split hit for this segment
      const EDEPHit segment_hit = create_segment_hit(segment_start_global, segment_end_global, segment_start_time,
                                                     time_direction, total_time_span, segment_fraction, hit);

      split_hit[current_wire] = segment_hit;

      cumulative_energy += segment_hit.GetEnergyDeposit();
      cumulative_secondary_energy += segment_hit.GetSecondaryDeposit();
      cumulative_length += segment_hit.GetTrackLength();
      last_hit_stop = segment_hit.GetStop();

      // Move to next segment
      segment_start_global = segment_end_global;
      segment_start_local  = segment_end_local;
      segment_start_time += time_direction * total_time_span * segment_fraction;
    }
    UFW_DEBUG(" Finished splitting hit into {} parts.", split_hit.size());
    return split_hit;
  }

  // TO-DO: For now identical to stt implementation, need to modify for drift specifics
  /**
   * \brief Builds the digis of every fired wire and appends them to the \c digi product.
   * \param hits_by_wire The hits of the event grouped per wire.
   * \todo Currently identical to the STT implementation; to be specialized for the drift chamber.
   */
  void drift_fast_digi::digitize_hits_in_wires(
      const std::map<const geoinfo::tracker_info::wire*, std::vector<EDEPHit>>& hits_by_wire) {
    auto& digi_out       = set<sand::tracker::digi_container>("digi");
    std::size_t nsignals = 0;

    for (const auto& [wire, hits] : hits_by_wire) {
      UFW_DEBUG("Station target: {}, station top north corner: {}", wire->parent->target, wire->parent->top_north);
      UFW_DEBUG(" Wire properties: Head {}, Tail {}", wire->head, wire->tail);
      UFW_DEBUG(" Number of hits in wire: {}", hits.size());

      auto wire_digis = process_hits_for_wire(hits, *wire);
      UFW_DEBUG(" Number of digis in wire: {}", wire_digis.size());
      for (auto& d : wire_digis) {
        nsignals += d.signals.size();
        digi_out.digis.emplace_back(std::move(d));
      }
    }
    UFW_INFO("Digitization complete. Created {} digis with {} signals in total.", digi_out.digis.size(), nsignals);
  }

  /**
   * \brief Applies the TDC resolution to a time.
   * \param time The true time [ns].
   * \return \p time smeared with a Gaussian of width \c sigma_tdc [ns].
   */
  double drift_fast_digi::smear_time(double time) {
    std::normal_distribution<double> gaussian_error(0.0, m_sigma_tdc); // FIXME should be member
    return time + gaussian_error(random_engine());
  }

  /**
   * \brief Computes the digis of a single wire from the hits assigned to it.
   *
   * For each hit the points of closest approach between the hit segment and the
   * wire are found and the arrival time at the readout end (hit time + drift time
   * + in-wire propagation time) is evaluated. Arrivals are sorted in time and
   * grouped into digis: a digi opens at the first arrival not yet used and
   * collects all arrivals within \c integration_time of it; the arrivals in the
   * following \c dead_time are discarded; the next arrival opens a new digi.
   * Each collected arrival becomes a signal holding the raw time, energy and hit
   * id. The digi TDC is the earliest signal time smeared by \c sigma_tdc, the
   * digi ADC the sum of the signal energies.
   *
   * \param hits The hits collected on the wire.
   * \param wire The geometry of the wire.
   * \return The time-ordered digis of the wire; empty if the wire has no hits.
   */
  tracker::digi_container::digi_collection
  drift_fast_digi::process_hits_for_wire(const std::vector<EDEPHit>& hits,
                                         const sand::geoinfo::drift_info::wire& wire) {
    struct arrival {
      double time;
      double edep;
      std::decay_t<decltype(std::declval<const EDEPHit&>().GetId())> hit_id;
    };

    std::vector<arrival> arrivals;
    arrivals.reserve(hits.size());
    for (const auto& hit : hits) {
      auto closest_points = closest_points_hit_wire(
          vec_4d(hit.GetStart().X(), hit.GetStart().Y(), hit.GetStart().Z(), hit.GetStart().T()),
          vec_4d(hit.GetStop().X(), hit.GetStop().Y(), hit.GetStop().Z(), hit.GetStop().T()), m_drift_velocity, wire);

      const vec_4d& closest_point_hit  = closest_points.first;
      const vec_4d& closest_point_wire = closest_points.second;

      const double arrival_time = get_min_time(closest_point_wire, m_wire_velocity, wire);
      UFW_DEBUG("  Hit {}: closest point on hit {}, on wire {}, arrival time {}", hit.GetId(), closest_point_hit,
                closest_point_wire, arrival_time);
      arrivals.push_back({arrival_time, hit.GetEnergyDeposit(), hit.GetId()});
    }

    std::stable_sort(arrivals.begin(), arrivals.end(),
                     [](const arrival& a, const arrival& b) { return a.time < b.time; });

    tracker::digi_container::digi_collection wire_digis;
    std::size_t i = 0;
    while (i < arrivals.size()) {
      const double window_start = arrivals[i].time;
      const double window_end   = window_start + m_integration_time;
      const double dead_end     = window_end + m_dead_time;

      // Signals: raw arrival time and deposited energy of every hit in [window_start, window_end).
      auto& current      = wire_digis.emplace_back();
      double digi_energy = 0.0;
      double digi_time   = std::numeric_limits<double>::max();
      for (; i < arrivals.size() && arrivals[i].time < window_end; ++i) {
        tracker::digi_container::digi::signal signal(wire.daq_channel, arrivals[i].time, arrivals[i].edep);
        signal.insert(arrivals[i].hit_id);
        digi_energy += signal.energy();
        digi_time = std::min(digi_time, signal.t());
        current.signals.push_back(std::move(signal));
      }

      // Digi: TDC = earliest signal time (smeared by sigma_tdc), ADC = sum of the signal energies.
      current.set_tdc(smear_time(digi_time));
      current.set_adc(digi_energy);
      UFW_DEBUG("  Digi in [{}, {}): TDC = {}, ADC = {}, {} signals.", window_start, window_end, current.tdc(),
                current.adc(), current.signals.size());

      for (; i < arrivals.size() && arrivals[i].time < dead_end; ++i)
        UFW_DEBUG("  Hit {} arrives at {} during dead time [{}, {}), discarded.", arrivals[i].hit_id,
                  arrivals[i].time, window_end, dead_end);
    }

    return wire_digis;
  }

  // TO-DO: For now identical to stt implementation, need to modify for drift specifics
  /**
   * \brief Finds the points of closest approach between a hit segment and a wire.
   *
   * Each returned point also carries the time at which it is reached: the hit
   * point keeps the interpolated hit time, the wire point adds the drift time
   * obtained from \p v_drift.
   *
   * \param hit_start Start point of the hit segment (space-time).
   * \param hit_stop  Stop point of the hit segment (space-time).
   * \param v_drift   Drift velocity used to set the time of the wire point [mm/ns].
   * \param w         The wire geometry.
   * \return A pair {closest point on the hit, closest point on the wire}.
   * \todo Currently identical to the STT implementation; to be specialized for the drift chamber.
   */
  std::pair<vec_4d, vec_4d> drift_fast_digi::closest_points_hit_wire(const vec_4d& hit_start,
                                                                     const vec_4d& hit_stop, // TO-DO move to fast_digi
                                                                     double v_drift,
                                                                     const geoinfo::tracker_info::wire& w) const {
    std::pair<vec_4d, vec_4d> closest_points;

    pos_3d start(hit_start.X(), hit_start.Y(), hit_start.Z());
    pos_3d stop(hit_stop.X(), hit_stop.Y(), hit_stop.Z());

    auto seg_params = w.closest_approach_segment(start, stop);

    std::vector<vec_4d> result;

    double& t       = seg_params.first;  // Parameter along s
    double& t_prime = seg_params.second; // Parameter along r

    // Calculate the closest point on the line segment
    pos_3d closest_point_hit = start + (stop - start) * t;

    if (t == 0 || t == 1) {
      dir_3d AP = closest_point_hit - w.head;
      t_prime   = AP.Dot(w.direction()) / w.direction().Mag2();
      t_prime   = std::max(0.0, std::min(1.0, t_prime));
    }

    // Calculate the corresponding point on the wire
    pos_3d closest_point_wire = w.head + w.direction() * t_prime;

    double fraction = sqrt((closest_point_hit - start).Mag2() / (stop - start).Mag2());
    vec_4d closest_point_hit_l(closest_point_hit.X(), closest_point_hit.Y(), closest_point_hit.Z(),
                               hit_start.T() + fraction * (hit_stop.T() - hit_start.T()));

    closest_points.first = closest_point_hit_l;

    vec_4d closest_point_wire_l(closest_point_wire.X(), closest_point_wire.Y(), closest_point_wire.Z(),
                                closest_point_hit_l.T()
                                    + sqrt((closest_point_hit - closest_point_wire).Mag2()) / v_drift);

    closest_points.second = closest_point_wire_l;

    return closest_points;
  }

  /**
   * \brief Earliest time a signal generated at \p point reaches the wire readout end.
   * \param point           Space-time point on the wire.
   * \param v_signal_inwire In-wire signal propagation speed [mm/ns].
   * \param w               The wire geometry; its \c head is the readout end.
   * \return The arrival time at the readout end [ns].
   */
  double drift_fast_digi::get_min_time(const vec_4d& point, double v_signal_inwire,
                                       const geoinfo::tracker_info::wire& w) const {
    return point.T() + sqrt((pos_3d(point.Vect()) - w.head).Mag2()) / v_signal_inwire;
  }
} // namespace sand::drift

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::drift::drift_fast_digi)
