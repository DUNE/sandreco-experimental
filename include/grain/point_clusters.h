#pragma once

#include <common/sand.h>


namespace sand::grain {

  struct point_clusters : managed_data_base {
    struct cluster {
      std::vector<point_cloud::point> clusterized_points;
      dir_3d line_direction;
      pos_3d line_point;
    };
    using point_clusters_list = std::vector<std::vector<cluster>>;
    point_clusters_list clusters;
  };

}

UFW_DECLARE_MANAGED_DATA(sand::grain::point_clusters)

