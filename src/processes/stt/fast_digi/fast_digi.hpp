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
    std::map<geo_id, std::vector<EDEPHit>> group_hits_by_tube();
    
    void digitize_hits_in_tubes(const std::map<geo_id, std::vector<EDEPHit>>& hits_by_tube);                             

    void update_timing_parameters(const EDEPHit& hit, 
                                  const sand::geoinfo::stt_info::wire& wire,
                                  const vec_4d& closest_point_hit,
                                 const vec_4d& closest_point_wire,
                                 double& wire_time,
                                 double& drift_time,
                                 double& signal_time,
                                 double& t_hit) const;

    tracker::digi::signal create_signal(double wire_time, double edep_total, const channel_id& channel);

    void log_hit_debug(const EDEPHit& hit);

    void log_tube_warning(std::string_view message, const geo_id& tube_id);

    void log_tube_debug(std::string_view message, const geo_id& tube_id);

    std::optional<tracker::digi::signal> process_hits_for_wire(
        const std::vector<EDEPHit>& hits, 
        const sand::geoinfo::stt_info::wire& wire,
        const geo_id& tube_id) ;

  private:
    double m_drift_velocity; //[mm/ns]
    double m_wire_velocity; //[mm/ns]
    double m_sigma_tdc; //[ns]

  };
  
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