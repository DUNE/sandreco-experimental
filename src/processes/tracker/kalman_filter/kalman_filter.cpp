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
    auto& tracks_out = set<tracks>("tracks");

    for(auto& tracklet_collection : tracklets_in.tracklets) {

      UFW_DEBUG("Tracklet plane: {}.", tracklet_collection.first);
      tracker::tracks::track_collection track_collection;

      for(const auto& tracklet : tracklet_collection.second) {

        UFW_DEBUG("Tracklet x: {}, theta_x: {}, y: {}, theta_y: {}",
                  tracklet.x, tracklet.theta_x, tracklet.y, tracklet.theta_y);

        tracker::tracks::track new_track;
        new_track.x = tracklet.x;
        new_track.theta_x = tracklet.theta_x;
        new_track.y = tracklet.y;
        new_track.theta_y = tracklet.theta_y;
        new_track.min = tracklet.min;
        new_track.err_x = tracklet.err_x;
        new_track.err_theta_x = tracklet.err_theta_x;
        new_track.err_y = tracklet.err_y;
        new_track.err_theta_y = tracklet.err_theta_y;
        track_collection.push_back(new_track);
      }
      tracks_out.track_map[tracklet_collection.first] = track_collection;
    }

    
  }





}
