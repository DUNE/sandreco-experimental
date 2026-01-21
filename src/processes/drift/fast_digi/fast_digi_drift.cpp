#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <edep_reader/edep_reader.hpp>
#include <geoinfo/drift_info.hpp>
#include <geoinfo/geoinfo.hpp>
#include <geoinfo/stt_info.hpp>
#include <geoinfo/tracker_info.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>
#include <tracker/digi.h>

#include <fast_digi_drift.hpp>

namespace sand::drift {

  void fast_digi::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_drift_velocity = cfg.at("drift_velocity");
    m_wire_velocity  = cfg.at("wire_velocity");
    m_sigma_tdc      = cfg.at("sigma_tdc");
  }

  fast_digi::fast_digi() : process({}, {{"digi", "sand::tracker::digi"}}) {
    UFW_DEBUG("Creating a fast_digi process at {}", fmt::ptr(this));
  }

  void fast_digi::run() {
    UFW_DEBUG("Running fast_digi process at {}", fmt::ptr(this));
    const auto& tree = get<sand::edep_reader>();
    const auto& gi   = get<geoinfo>();
    auto& digi       = set<sand::tracker::digi>("digi");
    auto& tgm        = ufw::context::current()->instance<root_tgeomanager>();
    std::map<const geoinfo::tracker_info::wire *, std::vector<EDEPHit>> hits_by_wire = group_hits_by_wire();
    digitize_hits_in_wires(hits_by_wire);
  }

  std::map<const geoinfo::tracker_info::wire *, std::vector<EDEPHit>> fast_digi::group_hits_by_wire() {
    const auto& gi   = get<geoinfo>();
    const auto& tree = get<sand::edep_reader>();
    auto& tgm        = ufw::context::current()->instance<root_tgeomanager>();
    std::map<const geoinfo::tracker_info::wire *, std::vector<EDEPHit>> hits_by_wire;
    const auto* drift = dynamic_cast<const sand::geoinfo::drift_info*>(&gi.tracker());

    for (const auto& trj : tree) {
      const auto& hit_map = trj.GetHitMap(); // pointer, not value
      if (!trj.HasHitInDetector(component::DRIFT)) 
        continue;

      UFW_INFO("Found {} DRIFT hits for trajectory with ID {}", hit_map.at(component::DRIFT).size(), trj.GetId());

      for(const auto& hit : hit_map.at(component::DRIFT)) {

        tgm.navigator()->FindNode(hit.GetStart().X(), hit.GetStart().Y(), hit.GetStart().Z());
        geo_path start_path(tgm.navigator()->GetPath());
        geo_path start_partial_path = drift->partial_path(start_path, gi);
        geo_id start_ID = drift->id(start_partial_path);

        tgm.navigator()->FindNode(hit.GetStop().X(), hit.GetStop().Y(), hit.GetStop().Z());
        geo_path stop_path(tgm.navigator()->GetPath());
        geo_path stop_partial_path = drift->partial_path(stop_path, gi);
        geo_id stop_ID = drift->id(stop_partial_path);

        if(start_ID.drift.plane == 255 || stop_ID.drift.plane == 255) {
          UFW_DEBUG(" One of two hit_segment ends has invalid plane ID, skipping.");
          continue;
        }
        if(start_ID.raw != stop_ID.raw) {
          UFW_DEBUG(" Hit spans multiple views: start view ID ({},{},{}), stop view ID ({},{},{}). Skipping.", 
                                start_ID.drift.subdetector, start_ID.drift.supermodule, start_ID.drift.plane, 
                                stop_ID.drift.subdetector, stop_ID.drift.supermodule, stop_ID.drift.plane);
          continue;
        }

        const auto* drift_station = static_cast<const sand::geoinfo::drift_info::station*>(drift->get_station_by_ID(start_ID.drift.supermodule));
        geoinfo::tracker_info::wire_list wires_in_view;
        if(start_ID.drift.plane == 0) {
          wires_in_view = drift_station->x_view();
        } else if (start_ID.drift.plane == 1) {
          wires_in_view = drift_station->u_view();
        } else if (start_ID.drift.plane == 2) {
          wires_in_view = drift_station->v_view();
        } 

        const auto [closest_wire_start, closest_wire_start_index] = drift->closest_wire_in_list(wires_in_view, pos_3d(hit.GetStart().Vect()));
        const auto [closest_wire_stop, closest_wire_stop_index] = drift->closest_wire_in_list(wires_in_view, pos_3d(hit.GetStop().Vect()));
        
        if(closest_wire_start_index == SIZE_MAX || closest_wire_stop_index == SIZE_MAX) {
          UFW_DEBUG(" Could not find closest wire index for one end of the hit, skipping.");
          continue;
        }
        if(closest_wire_start==nullptr || closest_wire_stop==nullptr) {
          UFW_DEBUG(" Could not find closest wire for one end of the hit, skipping.");
          continue;
        }    

        if(closest_wire_start==closest_wire_stop) {
          UFW_DEBUG(" For hit ID ({}) closest wire start and stop are the same wire: wire ID: ({}).", hit.GetId(), closest_wire_start_index);
          hits_by_wire[closest_wire_start].emplace_back(hit);
        } else {
          UFW_DEBUG(" For hit ID ({}) closest wire start and stop are different wires: wire ID: ({},{}).", hit.GetId(), closest_wire_start_index, closest_wire_stop_index);
          auto split_hits = split_hit(closest_wire_start_index, closest_wire_stop_index, wires_in_view, hit);
          UFW_DEBUG(" Split hit between {} wires.", split_hits.size());
          for (const auto& [wire, split_hit] : split_hits) {
            hits_by_wire[wire].emplace_back(split_hit);
          }
        }

      }
      
    }

    UFW_INFO("Hits have been grouped associated to {} wires.", hits_by_wire.size());
    return hits_by_wire;
  }

  double fast_digi::calculate_wire_boundary_transverse(const geoinfo::tracker_info::wire* current_wire,
                                                       const geoinfo::tracker_info::wire* next_wire,
                                                       const xform_3d& wire_plane_transform,
                                                       double transverse_start,
                                                       double transverse_end,
                                                       size_t wire_index) const {
    if (next_wire == nullptr) {
      return transverse_end;
    }

    // Calculate wire centers in local coordinates
    const pos_3d wire1_center_global = (current_wire->head + dir_3d(current_wire->tail)) * 0.5;
    const pos_3d wire1_center_local = wire_plane_transform.Inverse() * wire1_center_global;
    const double wire1_transverse = wire1_center_local.Y();

    const pos_3d wire2_center_global = (next_wire->head + dir_3d(next_wire->tail)) * 0.5;
    const pos_3d wire2_center_local = wire_plane_transform.Inverse() * wire2_center_global;
    const double wire2_transverse = wire2_center_local.Y();

    // Midpoint between adjacent wires defines the boundary
    const double boundary_transverse = 0.5 * (wire1_transverse + wire2_transverse);

    UFW_DEBUG(" Wire1 index: {}, transverse coord: {}", wire_index, wire1_transverse);
    UFW_DEBUG(" Wire2 index: {}, transverse coord: {}", wire_index + 1, wire2_transverse);
    UFW_DEBUG(" Boundary transverse coord: {}", boundary_transverse);

    // Check if boundary is between start and end, otherwise use end
    const double distance_to_boundary = fabs(boundary_transverse - transverse_start);
    const double distance_to_end = fabs(transverse_end - transverse_start);

    return (distance_to_boundary < distance_to_end) ? boundary_transverse : transverse_end;
  }

  void fast_digi::log_segment_debug(const pos_3d& segment_start_global,
                                    const pos_3d& segment_end_global,
                                    const pos_3d& segment_end_local,
                                    double segment_length,
                                    double segment_fraction,
                                    size_t wire_index) const {
    UFW_DEBUG(" ===== Segment {} Debug Info =====", wire_index);
    UFW_DEBUG("   Start global: {}", segment_start_global);
    UFW_DEBUG("   End global:   {}", segment_end_global);
    UFW_DEBUG("   End local:    {}", segment_end_local);
    UFW_DEBUG("   Length: {:.3f}, Fraction: {:.4f}", segment_length, segment_fraction);
  }

  std::pair<pos_3d, pos_3d> fast_digi::interpolate_segment_endpoint(const pos_3d& start_local,
                                                 double dx_local, double dy_local, double dz_local,
                                                 double segment_end_transverse,
                                                 const xform_3d& transform) const {
    // Linear interpolation parameter: fraction along hit segment
    const double interpolation_param = fabs((segment_end_transverse - start_local.Y()) / dy_local);

    // Calculate segment endpoint in local coordinates
    const pos_3d segment_end_local(
        start_local.X() + dx_local * interpolation_param,
        start_local.Y() + dy_local * interpolation_param,
        start_local.Z() + dz_local * interpolation_param);

    // Transform to global coordinates
    const pos_3d segment_end_global = transform * segment_end_local;

    return {segment_end_local, segment_end_global};
  }

  EDEPHit fast_digi::create_segment_hit(const pos_3d& segment_start_global,
                                        const pos_3d& segment_end_global,
                                        double segment_start_time,
                                        double time_direction,
                                        double total_time_span,
                                        double segment_fraction,
                                        const EDEPHit& original_hit) const {
    const vec_4d segment_start_4d(segment_start_global.X(), segment_start_global.Y(),
                                          segment_start_global.Z(), segment_start_time);
    const vec_4d segment_end_4d(segment_end_global.X(), segment_end_global.Y(),
                                        segment_end_global.Z(),
                                        segment_start_time + time_direction * total_time_span * segment_fraction);

    const double scaled_energy = original_hit.GetEnergyDeposit() * segment_fraction;
    const double scaled_secondary = original_hit.GetSecondaryDeposit() * segment_fraction;
    const double scaled_track_length = original_hit.GetTrackLength() * segment_fraction;

    return EDEPHit(segment_start_4d, segment_end_4d,
                   scaled_energy, scaled_secondary,
                   scaled_track_length, original_hit.GetContrib(),  // TO-DO: How to split contributor?
                   original_hit.GetPrimaryId(), original_hit.GetId());
  }

  std::map<const geoinfo::tracker_info::wire *, EDEPHit> fast_digi::split_hit(
                                          size_t closest_wire_start_index,
                                          size_t closest_wire_stop_index,
                                          const geoinfo::tracker_info::wire_list& wires_in_view,
                                          const EDEPHit& hit) {
    
    const auto& gi   = get<geoinfo>();
    const auto* drift = dynamic_cast<const sand::geoinfo::drift_info*>(&gi.tracker());
    std::map<const geoinfo::tracker_info::wire *, EDEPHit> split_hit;

    // Extract hit segment properties
    const double total_hit_length = sqrt((hit.GetStop().Vect() - hit.GetStart().Vect()).Mag2());
    const double total_time_span = (hit.GetStop() - hit.GetStart()).T();
    const double hit_start_time = hit.GetStart().T();

    // Get coordinate transformation for wire plane
    const xform_3d wire_plane_transform = wires_in_view.at(closest_wire_start_index)->wire_plane_transform();

    // Transform hit endpoints from global to local rotated coordinates
    const pos_3d hit_start_global = pos_3d(hit.GetStart().X(), hit.GetStart().Y(), hit.GetStart().Z());
    const pos_3d hit_start_local = wire_plane_transform.Inverse() * hit_start_global;

    const pos_3d hit_stop_global = pos_3d(hit.GetStop().X(), hit.GetStop().Y(), hit.GetStop().Z());
    const pos_3d hit_stop_local = wire_plane_transform.Inverse() * hit_stop_global;

    UFW_DEBUG(" Total hit key properties: Start {}, Stop {}, EnergyDeposit: {}, SecondaryDeposit: {}, TrackLength: {}, Contrib: {}, PrimaryId: {}, Id: {}", 
              vec_4d(hit.GetStart()), vec_4d(hit.GetStop()), hit.GetEnergyDeposit(), hit.GetSecondaryDeposit(), hit.GetTrackLength(), hit.GetContrib(), hit.GetPrimaryId(), hit.GetId());

    // Setup iteration parameters: ensure we iterate from first to last wire
    const bool need_to_reverse = (closest_wire_start_index > closest_wire_stop_index);
    
    size_t first_wire_index = need_to_reverse ? closest_wire_stop_index : closest_wire_start_index;
    size_t last_wire_index  = need_to_reverse ? closest_wire_start_index : closest_wire_stop_index;
    
    // Starting point for iteration (may be swapped if reversing)
    pos_3d segment_start_global = need_to_reverse ? hit_stop_global : hit_start_global;
    pos_3d segment_start_local = need_to_reverse ? hit_stop_local : hit_start_local;
    double segment_start_time = need_to_reverse ? (hit_start_time + total_time_span) : hit_start_time;
    double time_direction = need_to_reverse ? -1.0 : 1.0;
    
    // Transverse coordinates (Y in local frame) for interpolation
    const double transverse_start = segment_start_local.Y();
    const double transverse_end = need_to_reverse ? hit_start_local.Y() : hit_stop_local.Y();
    
    // Local coordinate deltas for linear interpolation
    const double dx_local = (need_to_reverse ? -1.0 : 1.0) * (hit_stop_local.X() - hit_start_local.X());
    const double dy_local = (need_to_reverse ? -1.0 : 1.0) * (hit_stop_local.Y() - hit_start_local.Y());
    const double dz_local = (need_to_reverse ? -1.0 : 1.0) * (hit_stop_local.Z() - hit_start_local.Z());
    
    if(need_to_reverse) {
      UFW_DEBUG(" Swapping closest wire start and stop to maintain order.");
    }



    // Iterate through wires, splitting hit into segments
    for(size_t wire_index = first_wire_index; wire_index <= last_wire_index; ++wire_index) {
      const auto* current_wire = wires_in_view.at(wire_index);
      const bool is_last_wire = (wire_index + 1 >= wires_in_view.size());
      const auto* next_wire = is_last_wire ? nullptr : wires_in_view.at(wire_index + 1);

      if(next_wire == nullptr) {
        UFW_DEBUG(" Reached last wire in view during hit splitting.");
        break;
      }

      // Determine the transverse coordinate where this segment ends
      const double segment_end_transverse = calculate_wire_boundary_transverse(
          current_wire, next_wire, wire_plane_transform,
          segment_start_local.Y(), transverse_end, wire_index);

      // Calculate segment endpoint in both local and global coordinates
      const auto [segment_end_local, segment_end_global] = interpolate_segment_endpoint(
          segment_start_local, dx_local, dy_local, dz_local,
          segment_end_transverse, wire_plane_transform);

      // Calculate what fraction of the total hit this segment represents
      const double segment_length = sqrt((segment_end_global - segment_start_global).Mag2());
      const double segment_fraction = segment_length / total_hit_length;

      // Validate segment fraction is reasonable
      if (segment_fraction < 0.0 || segment_fraction > 1.0) {
        UFW_WARN(" Segment fraction out of range [0,1]: {} for wire index {}", segment_fraction, wire_index);
      }

      log_segment_debug(segment_start_global, segment_end_global, segment_end_local,
                       segment_length, segment_fraction, wire_index);

      // Build split hit for this segment
      const EDEPHit segment_hit = create_segment_hit(
          segment_start_global, segment_end_global,
          segment_start_time, time_direction, total_time_span,
          segment_fraction, hit);

      split_hit[current_wire] = segment_hit;

      // Move to next segment
      segment_start_global = segment_end_global;
      segment_start_local = segment_end_local;
      segment_start_time += time_direction * total_time_span * segment_fraction;
    }
    UFW_DEBUG(" Finished splitting hit into {} parts.", split_hit.size());
    return split_hit; 
  }

  // TO-DO: For now identical to stt implementation, need to modify for drift specifics
  void fast_digi::digitize_hits_in_wires(const std::map<const geoinfo::tracker_info::wire *, std::vector<EDEPHit>>& hits_by_wire) {
    const auto& gi  = get<geoinfo>();
    auto& digi      = set<sand::tracker::digi>("digi");
    const auto* drift = dynamic_cast<const sand::geoinfo::drift_info*>(&gi.tracker());

    for (auto [wire, hits] : hits_by_wire) { 
      UFW_DEBUG("Station target: {}, station top north corner: {}", wire->parent->target, wire->parent->top_north);
      UFW_DEBUG(" Wire properties: Head {}, Tail {}", wire->head, wire->tail);
      UFW_DEBUG(" Number of hits in wire: {}", hits.size());
      
      auto signal = process_hits_for_wire(hits, *wire);
      if (signal) {
        std::for_each(hits.begin(), hits.end(), [&signal](const auto& hit) { signal->insert(hit.GetId()); });
        digi.signals.emplace_back(std::move(*signal));
      }
      
    }
    UFW_INFO("Digitization complete. Total number of signals created: {}", digi.signals.size());
  }

  tracker::digi::signal fast_digi::create_signal(double wire_time, double edep_total, const channel_id& channel) {
    std::normal_distribution<double> gaussian_error(0.0, m_sigma_tdc); //FIXME should be member
    auto ran = gaussian_error(random_engine());
    //FIXME replace 200 with maximum drift + signal time
    reco::digi::time trange{wire_time - 200., wire_time, wire_time + 5. * m_sigma_tdc};
    tracker::digi::signal signal{reco::digi{channel, trange}, wire_time + ran, edep_total};

    UFW_DEBUG("  Created signal: Channel(subdetector {}, channel {}), TDC = {}, ADC = {}",
              static_cast<int>(signal.channel().subdetector), static_cast<int>(signal.channel().channel), signal.tdc,
              signal.adc);

    return signal;
  }

  std::optional<tracker::digi::signal> fast_digi::process_hits_for_wire(const std::vector<EDEPHit>& hits,
                                                                        const sand::geoinfo::drift_info::wire& wire) {
    const auto& gi     = get<geoinfo>();
    const auto* drift    = dynamic_cast<const sand::geoinfo::drift_info*>(&gi.tracker());
    double wire_time   = std::numeric_limits<double>::max();
    double drift_time  = std::numeric_limits<double>::max();
    double signal_time = std::numeric_limits<double>::max();
    double t_hit       = std::numeric_limits<double>::max();
    double edep_total  = 0.0;

    for (const auto& hit : hits) {

      auto closest_points = closest_points_hit_wire(
          vec_4d(hit.GetStart().X(), hit.GetStart().Y(), hit.GetStart().Z(), hit.GetStart().T()),
          vec_4d(hit.GetStop().X(), hit.GetStop().Y(), hit.GetStop().Z(), hit.GetStop().T()), m_drift_velocity, wire);

      const vec_4d& closest_point_hit  = closest_points.first;
      const vec_4d& closest_point_wire = closest_points.second;

      // Update timing parameters directly here
      double hit_smallest_time = get_min_time(closest_point_hit, m_wire_velocity, wire);

      if (hit_smallest_time < wire_time) {
        wire_time   = hit_smallest_time;
        t_hit       = closest_point_hit.T();
        drift_time  = closest_point_wire.T() - t_hit;
        signal_time = hit_smallest_time - closest_point_wire.T();

        UFW_DEBUG("Closest point on hit: {}", vec_4d(closest_point_hit));
        UFW_DEBUG("Closest point on wire: {}", vec_4d(closest_point_wire));
      }
      edep_total += hit.GetEnergyDeposit();
    }

    if (wire_time == std::numeric_limits<double>::max()) {
      return std::nullopt;
    }

    return create_signal(wire_time, edep_total, wire.daq_channel);
  }

  // TO-DO: For now identical to stt implementation, need to modify for drift specifics
  std::pair<vec_4d,vec_4d> fast_digi::closest_points_hit_wire(const vec_4d& hit_start, const vec_4d& hit_stop,  //TO-DO move to fast_digi
                                                            double v_drift, const geoinfo::tracker_info::wire& w) const {
    std::pair<vec_4d,vec_4d> closest_points;

    pos_3d start(hit_start.X(), hit_start.Y(), hit_start.Z());
    pos_3d stop(hit_stop.X(), hit_stop.Y(), hit_stop.Z());

    auto seg_params = w.closest_approach_segment(start, stop);

    std::vector<vec_4d> result;

    double& t       = seg_params.first; // Parameter along s
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

  double fast_digi::get_min_time(const vec_4d& point, double v_signal_inwire, const geoinfo::tracker_info::wire& w) const {
    return point.T() + sqrt((pos_3d(point.Vect()) - w.head).Mag2()) / v_signal_inwire;
  }
} // namespace sand::drift
