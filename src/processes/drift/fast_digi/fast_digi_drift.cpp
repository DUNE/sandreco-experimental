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

        const geoinfo::tracker_info::wire* closest_wire_start = drift->closest_wire_in_list(wires_in_view, hit.GetStart(), m_drift_velocity);
        const geoinfo::tracker_info::wire* closest_wire_stop = drift->closest_wire_in_list(wires_in_view, hit.GetStop(), m_drift_velocity);
        
        if(closest_wire_start==nullptr || closest_wire_stop==nullptr) {
          UFW_WARN(" Could not find closest wire for one end of the hit, skipping.");
          continue;
        }

        if(closest_wire_start==closest_wire_stop) UFW_INFO(" Closest wire start and stop are the same wire.");
        else UFW_INFO(" Closest wire start and stop are different wires.");



        hits_by_wire[closest_wire_start].emplace_back(hit);
      }
      
    }

    return hits_by_wire;
  }

} // namespace sand::stt
