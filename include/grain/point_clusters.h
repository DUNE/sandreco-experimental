#pragma once

#include <common/hit.h>
#include <common/sand.h>
#include <common/timerange.h>

namespace sand::grain {

  struct point_clusters : managed_data_base {
    class cluster : public reco::hit {
     public:
      /// @brief Default constuctor produces an invalid cluster, required by ROOT, do not use
      cluster() : reco::hit(pos_3d(0.0, 0.0, 0.0), dir_3d(0.0, 0.0, 0.0), reco::timerange(0.0, 0.0), 0.0) {}
      cluster(const pos_3d& c, const dir_3d& d, const reco::timerange& t, double w,
              const std::vector<point_cloud::point>& points)
        : reco::hit(c, d, t, w), m_clusterized_points(points) {}
      std::vector<point_cloud::point> points() const { return m_clusterized_points; }

     private:
      std::vector<point_cloud::point> m_clusterized_points;
    };
    using point_clusters_list = std::vector<std::vector<cluster>>;
    point_clusters_list clusters;
  };

} // namespace sand::grain

UFW_DECLARE_MANAGED_DATA(sand::grain::point_clusters)
