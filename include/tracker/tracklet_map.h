#pragma once

#include <vector>
#include <cstdint>

#include <ufw/data.hpp>
#include <common/truth.h>

namespace sand::tracker {

  struct tracklet_map : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    struct tracklet
    {
      double x;
      double theta_x;
      double y;
      double theta_y;
      double min;
      double err_x;
      double err_theta_x;
      double err_y;
      double err_theta_y;
    };

    using tracklet_collection = std::vector<tracklet>;
    
    std::map<double, tracklet_collection> tracklets;
  };

}

UFW_DECLARE_MANAGED_DATA(sand::tracker::tracklet_map)
