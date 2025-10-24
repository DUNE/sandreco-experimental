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

namespace sand::stt {

  class fast_digi : public ufw::process {

  public:
    fast_digi();
    void configure (const ufw::config& cfg) override;
    void run() override;

  private:
    double m_drift_velocity; //[mm/ns]
    double m_wire_velocity; //[mm/ns]
    double m_sigma_tdc; //[ns]

  };

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
    auto nav = tgm.navigator();
    const sand::geoinfo::tracker_info &tracker_ref = gi.tracker();

    //UFW_DEBUG(" subdetector type is {}", static_cast<int>(gi.tracker().subdetector()));
    if(gi.tracker().subdetector() == subdetector_t::STT){
      UFW_DEBUG(" STT subdetector implementation");
      auto* stt = dynamic_cast<const sand::geoinfo::stt_info*>(&tracker_ref);
      for (const auto& trj : tree) {
        const auto& hit_map = trj.GetHitMap(); // pointer, not value    
        if(hit_map.find(component::STRAW) == hit_map.end()) continue;

        UFW_DEBUG("Found {} STRAW hits for trajectory with ID {}", hit_map.at(component::STRAW).size(), trj.GetId());
    
        for (const auto& hit : hit_map.at(component::STRAW)){

          pos_3d hit_mid_point = pos_3d((hit.GetStart().X() + hit.GetStop().X()) * 0.5,
                                        (hit.GetStart().Y() + hit.GetStop().Y()) * 0.5,
                                        (hit.GetStart().Z() + hit.GetStop().Z()) * 0.5);

          nav->FindNode(hit_mid_point.x(), hit_mid_point.y(), hit_mid_point.z());

          geo_path node_path(nav->GetPath());
          geo_path partial_path = node_path - gi.root_path();
          geo_id ID = stt->id(partial_path); 

          UFW_DEBUG("Hit at position ({}, {}, {}) is in geometry node {}.", hit_mid_point.X(), hit_mid_point.Y(), hit_mid_point.Z(), partial_path);
          UFW_DEBUG("Corresponding geo_id: subdetector {}, supermodule {}, station {}, straw {}.",
                    static_cast<int>(ID.stt.subdetector),
                    static_cast<int>(ID.stt.supermodule),
                    static_cast<int>(ID.stt.plane),
                    static_cast<int>(ID.stt.tube));

        //   if(nav->FindNode(hit_mid_point.X(), hit_mid_point.Y(), hit_mid_point.Z())){
        //     geo_path node_path = nav->GetPath();
        //     geo_id ID = tracker_ref.id(node_path); 

        //     // UFW_DEBUG("Hit at position ({}, {}, {}) is in geometry node.", hit_mid_point.X(), hit_mid_point.Y(), hit_mid_point.Z());
        //     } else {
        // //       UFW_WARN("Could not find geometry node for hit at position ({}, {}, {}).", hit_mid_point.X(), hit_mid_point.Y(), hit_mid_point.Z());
        // //       continue;
        //     }
          }
        }     

      } else if (gi.tracker().subdetector() == subdetector_t::DRIFT) {
        UFW_ERROR("Drift detector not yet supported in fast_digi.");
      } else {
        UFW_ERROR("Unknown tracker subdetector type.");
      }


  }
}

UFW_REGISTER_PROCESS(sand::stt::fast_digi)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::stt::fast_digi)


//}
// UFW_DEBUG("Trajectory corresponds to STT subdetector.");
// for(auto t=0; t<stt->stations().size();t++){
//   const auto * station_ptr = stt->stations().at(t).get();
//   if(auto* stt_station = static_cast<const sand::geoinfo::stt_info::station*>(station_ptr)){
//     UFW_DEBUG("STT station {} with {} wires.", t, stt_station->wires.size());
//     for(auto w=0; w<stt_station->wires.size();w++){
//       const auto * wire_ptr = stt_station->wires.at(w).get();
//       if(auto* stt_wire_ptr = static_cast<const sand::geoinfo::stt_info::wire*>(wire_ptr)){
//         UFW_DEBUG("STT wire {} with max radius {}.", w, stt_wire_ptr->max_radius);
//       }
//     }
//   }
// }