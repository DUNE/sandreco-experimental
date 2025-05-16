#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <tracker/tracklets.h>
#include <tracker/tracklets.h>

#include <kalman_filter.hpp>

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::kalman_filter)

namespace sand::tracker {

  kalman_filter::kalman_filter() : process({{"tracklets", "sand::tracker::tracklets"}}, {{"tracks", "sand::tracker::tracks"}}), m_uniform(0.0, 1.0) {
    UFW_DEBUG("Creating a kalman_filter process at {}", fmt::ptr(this));
  }

  void kalman_filter::configure (const ufw::config& cfg) {
    process::configure(cfg);
    m_trackletinfo_placeholder = cfg.at("trackletinfo_placeholder"); 
  }


  void kalman_filter::run() {
    m_stat_photons_processed = 0;
    m_stat_photons_accepted = 0;
    m_stat_photons_discarded = 0;
    const auto& hits_in = get<tracklets>("tracklets");
    UFW_DEBUG("Hits size: {}.", hits_in.cameras.size());
    auto& tracks_out = set<tracks>("tracks");

    for (auto& cam : hits_in.cameras) {
      UFW_DEBUG("Camera {} has {} photon hits.", cam.camera_name, cam.photons.size());
      digi_out.cameras.emplace_back(assign_to_pixel(cam));  
    }
    UFW_INFO("Processed {} photons; {} were accepted, {} discarded.", m_stat_photons_processed, m_stat_photons_accepted, m_stat_photons_discarded);
  }





}
