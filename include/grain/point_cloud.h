#pragma once

#include <common/sand.h>


namespace sand::grain {

  struct point_cloud : managed_data_base {
    struct point {
      pos_3d position;
      double amplitude;
    };
    using point_cloud_list = std::vector<std::vector<point>>;
    point_cloud_list points;
  };

}

UFW_DECLARE_MANAGED_DATA(sand::grain::point_cloud)

