#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <tracker/tracklet_map.h>
#include <tracker/tracks.h>

#include <kalman_filter.hpp>

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::tracker::kalman_filter)

namespace sand::tracker {

  kalman_filter::kalman_filter() : process({{"tracklet_map", "sand::tracker::tracklet_map"}}, {{"tracks", "sand::tracker::tracks"}}) {
    UFW_DEBUG("Creating a kalman_filter process at {}", fmt::ptr(this));
  }

  void kalman_filter::configure (const ufw::config& cfg) {
    process::configure(cfg);
    m_trackletinfo_placeholder = cfg.at("trackletinfo_placeholder"); 
  }


  void kalman_filter::run() {

    const auto& tracklets_in = get<tracklet_map>("tracklet_map");

    for(auto& tracklet_collection : tracklets_in.tracklets) {

      UFW_DEBUG("Tracklet plane: {}.", tracklet_collection.first);

      for(const auto& tracklet : tracklet_collection.second) {

        UFW_DEBUG("Tracklet x: {}, theta_x: {}, y: {}, theta_y: {}",
                  tracklet.x, tracklet.theta_x, tracklet.y, tracklet.theta_y);

      }
    }

    auto& tracks_out = set<tracks>("tracks");
    
  }





}
