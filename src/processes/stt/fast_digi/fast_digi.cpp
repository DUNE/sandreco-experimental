#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <tracker/digi.h>
#include <edep_reader/edep_reader.hpp>
#include <geoinfo/geoinfo.hpp>
#include <geoinfo/tracker_info.hpp>
#include <geoinfo/stt_info.hpp>
#include <geoinfo/drift_info.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>

#include <fast_digi.hpp>

namespace sand::stt {

  void fast_digi::configure (const ufw::config& cfg) {
    process::configure(cfg); 
    m_drift_velocity = cfg.at("drift_velocity");
    m_wire_velocity = cfg.at("wire_velocity");
    m_sigma_tdc = cfg.at("sigma_tdc");
  }

  fast_digi::fast_digi() : process({}, {{"digi", "sand::tracker::digi"}}) {
    UFW_DEBUG("Creating a fast_digi process at {}", fmt::ptr(this));
  } 

  void fast_digi::run() {

    UFW_DEBUG("Running fast_digi process at {}", fmt::ptr(this));
    const auto& tree = get<sand::edep_reader>();
    const auto& gi = get<geoinfo>();
    auto& digi = set<sand::tracker::digi>("digi");
    auto& tgm = ufw::context::current()->instance<root_tgeomanager>();
    std::map<geo_id, std::vector<EDEPHit>> hits_by_tube;

    if(gi.tracker().subdetector() == subdetector_t::STT){
      UFW_DEBUG(" STT subdetector implementation");
      group_hits_by_tube(hits_by_tube, gi, tree, tgm);   

    } else {
      UFW_ERROR("Unknown tracker subdetector type.");
    }
  }

  void fast_digi::group_hits_by_tube(std::map<geo_id, std::vector<EDEPHit>>& hits_by_tube, 
                                      const sand::geoinfo & gi, const sand::edep_reader & tree, 
                                      sand::root_tgeomanager & tgm) {
    // Implementation of hit grouping by tube goes here
    const auto* stt = dynamic_cast<const sand::geoinfo::stt_info*>(&gi.tracker());
    for (const auto& trj : tree) {
        const auto& hit_map = trj.GetHitMap(); // pointer, not value    
        if(hit_map.find(component::STRAW) == hit_map.end()) continue;

        UFW_DEBUG("Found {} STRAW hits for trajectory with ID {}", hit_map.at(component::STRAW).size(), trj.GetId());
    
        for (const auto& hit : hit_map.at(component::STRAW)){

          pos_3d hit_mid_point = pos_3d((hit.GetStart().X() + hit.GetStop().X()) * 0.5,
                                        (hit.GetStart().Y() + hit.GetStop().Y()) * 0.5,
                                        (hit.GetStart().Z() + hit.GetStop().Z()) * 0.5);

          tgm.navigator()->FindNode(hit_mid_point.x(), hit_mid_point.y(), hit_mid_point.z());

          geo_path node_path(tgm.navigator()->GetPath());
          geo_path partial_path = node_path - gi.root_path();
          geo_id ID = stt->id(partial_path); 

          UFW_DEBUG("Hit at position ({}, {}, {}) is in geometry node {}.", hit_mid_point.X(), hit_mid_point.Y(), hit_mid_point.Z(), partial_path);
          UFW_DEBUG("Corresponding geo_id: subdetector {}, supermodule {}, station {}, straw {}.",
                    static_cast<int>(ID.stt.subdetector),
                    static_cast<int>(ID.stt.supermodule),
                    static_cast<int>(ID.stt.plane),
                    static_cast<int>(ID.stt.tube));
          }
        }
    
  }
}
