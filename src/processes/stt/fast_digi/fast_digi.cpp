#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <tracker/digi.h>
#include <edep_reader/edep_reader.hpp>
#include <geoinfo/geoinfo.hpp>
#include <geoinfo/tracker_info.hpp>

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
      const auto& hit_map = trj.GetHitMap();
      sand::tracker::digi::signal signal;
      signal.adc = 0.0;
      for (const auto& hit : hit_map.at(component::STRAW)){
        signal.adc += hit.GetEnergyDeposit();
      }
      digi.signals.push_back(signal);
    }

  }
}
UFW_REGISTER_PROCESS(sand::stt::fast_digi)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::stt::fast_digi)
