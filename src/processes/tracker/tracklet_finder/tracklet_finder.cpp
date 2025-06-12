#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <tracklet_finder.hpp>

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::tracker::tracklet_finder)

namespace sand::tracker {

  tracklet_finder::tracklet_finder() : process({{"digits", "sand::tracker::digi"}}, {{"tracklets", "sand::tracker::tracklet_collection"}}) {
    UFW_DEBUG("Creating a tracklet_finder process at {}", fmt::ptr(this));
  }

  void tracklet_finder::configure (const ufw::config& cfg) {
    process::configure(cfg); 

  }

  void tracklet_finder::run() {
  }

}
