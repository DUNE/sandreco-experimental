#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <tracker/tracklet_map.h>
#include <tracker/tracks.h>
#include <tracker/s_particle_infos.h>

#include <kalman_filter.hpp>
#include <Track.hh>

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::tracker::kalman_filter)

namespace sand::tracker {

  kalman_filter::kalman_filter() : process({{"tracklet_map", "sand::tracker::tracklet_map"} , {"s_particle_infos", "sand::tracker::s_particle_infos"}}, {{"tracks", "sand::tracker::tracks"}}) {
    UFW_DEBUG("Creating a kalman_filter process at {}", fmt::ptr(this));
  }

  void kalman_filter::configure (const ufw::config& cfg) {
    process::configure(cfg);
    m_trackletinfo_placeholder = cfg.at("trackletinfo_placeholder"); 
  }


  void kalman_filter::run() {

    const auto& tracklets_in = get<tracklet_map>("tracklet_map"); 
    const auto& s_particle_infos_in = get<s_particle_infos>("s_particle_infos");   
    auto& tracks_out = set<tracks>("tracks");

    for(auto& tracklet_collection : tracklets_in.tracklets) {

      UFW_DEBUG("Tracklet plane: {}.", tracklet_collection.first);

      for(const auto& tracklet : tracklet_collection.second) {

        UFW_DEBUG("Tracklet x: {}, theta_x: {}, y: {}, theta_y: {}",
                  tracklet.x, tracklet.theta_x, tracklet.y, tracklet.theta_y);

        tracker::tracks::track new_track;
        tracks_out.tracks.push_back(new_track);
      }
    }

    
  }





}
