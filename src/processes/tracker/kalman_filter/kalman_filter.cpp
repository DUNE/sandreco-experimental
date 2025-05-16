#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <tracker/tracklets.h>
#include <tracker/tracks.h>

#include <kalman_filter.hpp>

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::tracker::kalman_filter)

namespace sand::tracker {

  kalman_filter::kalman_filter() : process({{"tracklets", "sand::tracker::tracklets"}}, {{"tracks", "sand::tracker::tracks"}}) {
    UFW_DEBUG("Creating a kalman_filter process at {}", fmt::ptr(this));
  }

  void kalman_filter::configure (const ufw::config& cfg) {
    process::configure(cfg);
    m_trackletinfo_placeholder = cfg.at("trackletinfo_placeholder"); 
  }


  void kalman_filter::run() {
    const auto& tracklets_in = get<tracklets>("tracklets");
    UFW_DEBUG("Hits size: {}.", tracklets_in.tracklets.size());
    auto& tracks_out = set<tracks>("tracks");
    UFW_INFO("Info");
  }





}
