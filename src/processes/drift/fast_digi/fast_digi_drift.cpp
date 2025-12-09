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

    UFW_DEBUG(" DRIFT subdetector implementation");
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

      UFW_DEBUG("Found {} DRIFT hits for trajectory with ID {}", hit_map.at(component::DRIFT).size(), trj.GetId());

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
          UFW_WARN(" One of two hit_segment ends has invalid plane ID, skipping.");
          continue;
        }
        if(start_ID.raw != stop_ID.raw) {
          UFW_WARN(" Hit spans multiple views: start view ID ({},{},{}), stop view ID ({},{},{}). Skipping.", 
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

        const auto [closest_wire_start, closest_wire_start_index] = drift->closest_wire_in_list(wires_in_view, hit.GetStart(), m_drift_velocity);
        const auto [closest_wire_stop, closest_wire_stop_index] = drift->closest_wire_in_list(wires_in_view, hit.GetStop(), m_drift_velocity);
        
        if(closest_wire_start_index == SIZE_MAX || closest_wire_stop_index == SIZE_MAX) {
          UFW_WARN(" Could not find closest wire index for one end of the hit, skipping.");
          continue;
        }
        if(closest_wire_start==nullptr || closest_wire_stop==nullptr) {
          UFW_WARN(" Could not find closest wire for one end of the hit, skipping.");
          continue;
        }    

        if(closest_wire_start==closest_wire_stop) {
          UFW_INFO(" Closest wire start and stop are the same wire.");
          hits_by_wire[closest_wire_start].emplace_back(hit);
        } else {
          UFW_INFO(" Closest wire start and stop are different wires.");
          auto split_hits = split_hit(closest_wire_start_index, closest_wire_stop_index, wires_in_view, hit);
          UFW_INFO(" Split hit into {} parts.", split_hits.size());
          for (const auto& [wire, split_hit] : split_hits) {
            hits_by_wire[wire].emplace_back(split_hit);
          }
        }

      }
      
    }

    return hits_by_wire;
  }

  std::map<const geoinfo::tracker_info::wire *, EDEPHit> fast_digi::split_hit(
                                          size_t closest_wire_start_index,
                                          size_t closest_wire_stop_index,
                                          geoinfo::tracker_info::wire_list wires_in_view,
                                          const EDEPHit& hit) {
    
    const auto& gi   = get<geoinfo>();
    const auto* drift = dynamic_cast<const sand::geoinfo::drift_info*>(&gi.tracker());
    std::map<const geoinfo::tracker_info::wire *, EDEPHit> split_hit;

    size_t first_in_list = closest_wire_start_index;
    size_t last_in_list  = closest_wire_stop_index;
    if(closest_wire_start_index >closest_wire_stop_index) {
      UFW_DEBUG(" Swapping closest wire start and stop to maintain order.");
      first_in_list = closest_wire_stop_index;
      last_in_list  = closest_wire_start_index;
    }

    double hseg_length = (hit.GetStop().Vect() - hit.GetStart().Vect()).Mag();
    double hseg_dt = (hit.GetStop() - hit.GetStart()).T();
    double hseg_start_t = hit.GetStart().T();

    xform_3d wire_plane_transform = wires_in_view.at(first_in_list)->wire_plane_transform();

    pos_3d hit_start_global = pos_3d(hit.GetStart().X(), hit.GetStart().Y(), hit.GetStart().Z());
    pos_3d hit_start_local_rotated = wire_plane_transform.Inverse() * hit_start_global;

    pos_3d hit_stop_global = pos_3d(hit.GetStop().X(), hit.GetStop().Y(), hit.GetStop().Z());
    pos_3d hit_stop_local_rotated = wire_plane_transform.Inverse() * hit_stop_global;

    auto delta_x_local_rotated = hit_stop_local_rotated.X() - hit_start_local_rotated.X();
    auto delta_y_local_rotated = hit_stop_local_rotated.Y() - hit_start_local_rotated.Y();
    auto delta_z_local_rotated = hit_stop_local_rotated.Z() - hit_start_local_rotated.Z();

    auto transverse_coord_start = hit_start_local_rotated.Y();

    UFW_INFO(" Hit start global: ({}, {}, {})", hit_start_global.X(), hit_start_global.Y(), hit_start_global.Z());
    UFW_INFO(" Hit start local rotated: ({}, {}, {})", hit_start_local_rotated.X(), hit_start_local_rotated.Y(), hit_start_local_rotated.Z());
    UFW_INFO(" Hit stop global: ({}, {}, {})", hit_stop_global.X(), hit_stop_global.Y(), hit_stop_global.Z());
    UFW_INFO(" Hit stop local rotated: ({}, {}, {})", hit_stop_local_rotated.X(), hit_stop_local_rotated.Y(), hit_stop_local_rotated.Z());
    UFW_INFO(" Delta X local rotated: {}", delta_x_local_rotated);
    UFW_INFO(" Delta Y local rotated: {}", delta_y_local_rotated);

    UFW_DEBUG(" Total hit key properties: Start ({}, {}, {}, {}), Stop ({}, {}, {}, {}), EnergyDeposit: {}, SecondaryDeposit: {}, TrackLength: {}, Contrib: {}, PrimaryId: {}, Id: {}", 
              hit.GetStart().X(), hit.GetStart().Y(), hit.GetStart().Z(), hit.GetStart().T(),
              hit.GetStop().X(), hit.GetStop().Y(), hit.GetStop().Z(), hit.GetStop().T(),
              hit.GetEnergyDeposit(), hit.GetSecondaryDeposit(), hit.GetTrackLength(), hit.GetContrib(), hit.GetPrimaryId(), hit.GetId());

    /// Set starting point
    auto start = hit_start_global;
    auto start_time = hseg_start_t;

    for(size_t wire_index = first_in_list; wire_index <= last_in_list; ++wire_index) {
      const auto* wire1 = wires_in_view.at(wire_index);
      const auto* wire2 = (wire_index + 1 <= wires_in_view.size() -1) ? wires_in_view.at(wire_index + 1) : nullptr; 

      if(wire2 == nullptr) {
        UFW_DEBUG(" Reached last wire in view during hit splitting.");
        break;
      }

      double step_coordinate = 0.0;
      // Calculate the plane coordinate between wire1 and wire2

      if (wire_index + 1 != wires_in_view.size()) {

        pos_3d global_wire1_center = (wire1->head + dir_3d(wire1->tail)) * 0.5;
        pos_3d local_wire1_center_rotated = wire_plane_transform.Inverse() * global_wire1_center;
        auto transverse_coord1 = local_wire1_center_rotated.Y();

        pos_3d global_wire2_center = (wire2->head + dir_3d(wire2->tail)) * 0.5;
        pos_3d local_wire2_center_rotated = wire_plane_transform.Inverse() * global_wire2_center;
        auto transverse_coord2 = local_wire2_center_rotated.Y();

        auto inner_plane_center_transverse_coord = 0.5 * (transverse_coord1 + transverse_coord2);

        UFW_DEBUG(" Wire1 index: {}, transverse coord: {}", wire_index, transverse_coord1);
        UFW_DEBUG(" Wire2 index: {}, transverse coord: {}", wire_index+1, transverse_coord2);
        UFW_DEBUG(" Inner plane center transverse coord: {}", inner_plane_center_transverse_coord);
        if (fabs(inner_plane_center_transverse_coord - transverse_coord_start) < 
            fabs(hit_stop_local_rotated.Y() - transverse_coord_start)) {
          step_coordinate = inner_plane_center_transverse_coord;
        } else {
          step_coordinate = hit_stop_local_rotated.Y();
        }

      } else {
        step_coordinate = hit_stop_local_rotated.Y();
      }

      double t = fabs((step_coordinate - transverse_coord_start) / delta_y_local_rotated);

      pos_3d rotated_crossing_point(hit_start_local_rotated.X() + delta_x_local_rotated * t, 
                                    hit_start_local_rotated.Y() + delta_y_local_rotated * t,
                                    hit_start_local_rotated.Z() + delta_z_local_rotated * t);

      pos_3d global_crossing_point = wire_plane_transform * rotated_crossing_point;
      auto stop = global_crossing_point;


      double segment_portion = sqrt((stop - start).Mag2()) / hseg_length;

      pos_3d mid_hit_seg_portion = (stop + dir_3d(start)) / 2.0; // TO-DO: Start should be the previous stopping point
      vec_4d mid_hit_seg_portion_4d(mid_hit_seg_portion.X(),mid_hit_seg_portion.Y(),mid_hit_seg_portion.Z(),start_time);
      
      const auto [closest_wire_to_position,closest_to_position_ind] = drift->closest_wire_in_list(wires_in_view,mid_hit_seg_portion_4d,m_drift_velocity);


      UFW_DEBUG(" Crossing point local rotated: ({}, {}, {})", rotated_crossing_point.X(), rotated_crossing_point.Y(), rotated_crossing_point.Z());
      UFW_DEBUG(" Crossing point global: ({}, {}, {})", global_crossing_point.X(), global_crossing_point.Y(), global_crossing_point.Z());
      UFW_DEBUG(" Segment portion: {}", segment_portion);
      UFW_DEBUG(" Hit Portion associated to wire index: {}", wire_index);

      /// Build new hit portion
      TLorentzVector hit_start_lv(start.X(), start.Y(), start.Z(), start_time);
      TLorentzVector hit_stop_lv(stop.X(), stop.Y(), stop.Z(), start_time + hseg_dt * segment_portion);
      auto energy_deposit_portion = hit.GetEnergyDeposit() * segment_portion;
      auto secondary_deposit_portion = hit.GetSecondaryDeposit() * segment_portion;
      auto track_length_portion = hit.GetTrackLength() * segment_portion;
      auto contrib_portion = hit.GetContrib(); // TO-DO: How to split contributor?
      auto primary_id = hit.GetPrimaryId();
      auto ID = hit.GetId();
      
      EDEPHit hit_split(hit_start_lv, hit_stop_lv, 
                        energy_deposit_portion, secondary_deposit_portion, 
                        track_length_portion, contrib_portion, primary_id, ID); 

      split_hit[closest_wire_to_position] = hit_split;

      hit_start_global = global_crossing_point;
    }
    return split_hit; // Return the original hit for now
  }

} // namespace sand::stt
