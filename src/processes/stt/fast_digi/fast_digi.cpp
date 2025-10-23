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

    // Placeholder non-sense algorithm to fill digi signals
    for (const auto& trj : tree) {
      UFW_DEBUG("Processing trajectory with ID {}", trj.GetId());
      const auto& hit_map = trj.GetHitMap(); // pointer, not value
      const sand::geoinfo::tracker_info &tracker_ref = gi.tracker();

      if (auto* stt = dynamic_cast<const sand::geoinfo::stt_info*>(&tracker_ref)) {
        UFW_DEBUG("Trajectory corresponds to STT subdetector.");
        for(auto t=0; t<stt->stations().size();t++){
          const auto * station_ptr = stt->stations().at(t).get();
          if(auto* stt_station = static_cast<const sand::geoinfo::stt_info::station*>(station_ptr)){
            UFW_DEBUG("STT station {} with {} wires.", t, stt_station->wires.size());
            for(auto w=0; w<stt_station->wires.size();w++){
              const auto * wire_ptr = stt_station->wires.at(w).get();
              if(auto* stt_wire_ptr = static_cast<const sand::geoinfo::stt_info::wire*>(wire_ptr)){
                UFW_DEBUG("STT wire {} with max radius {}.", w, stt_wire_ptr->max_radius);
              }
            }
          }
        }
      } else if (auto* drift = dynamic_cast<const sand::geoinfo::drift_info*>(&tracker_ref)) {
        UFW_ERROR("Drift detector not yet supported in fast_digi.");
      } else {
        UFW_ERROR("Unknown tracker subdetector type.");
      }

      // sand::tracker::digi::signal signal;
      // signal.adc = 0.0;
      // try {
      // for (const auto& hit : hit_map.at(component::STRAW)){
      //   signal.adc += hit.GetEnergyDeposit();
      //   sand::geoinfo::stt_info stt = gi.tracker();
      // }
      // }catch(std::exception&){}
      // digi.signals.push_back(signal);
    }

  }
}
UFW_REGISTER_PROCESS(sand::stt::fast_digi)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::stt::fast_digi)
