#pragma once

#include <common/hit.h>
#include <common/sand.h>
#include <common/timerange.h>
#include <grain/point_cloud.h>

namespace sand::grain {

  struct point_clusters : managed_data_base {
    class cluster : public reco::hit {
     public:
      /// @brief Default constuctor produces an invalid cluster, required by ROOT, do not use
      cluster() = default;

      template <typename... Args>
      static std::unique_ptr<cluster> create(Args&&... args) {
        return shared::create<cluster>(std::forward<Args>(args)...);
      }

      std::vector<point_cloud::point> points() const { return m_clusterized_points; }

    protected:
      friend class reco::shared;

      cluster(const pos_3d& c, const dir_3d& d, const reco::timerange& t, double w,
              const std::vector<point_cloud::point>& points)
        : reco::hit(c, d, t, w), m_clusterized_points(points) {}

      /// Used by reco::shared::split and derived classes to make a copy, do not use
      cluster(const cluster&) = default;

     private:
      reco::shared* copy_self() const override { return new cluster(*this); }

      std::vector<point_cloud::point> m_clusterized_points;
    };
    using point_clusters_list = std::vector<std::vector<std::unique_ptr<cluster>>>;
    point_clusters_list clusters;
  };

} // namespace sand::grain

UFW_DECLARE_MANAGED_DATA(sand::grain::point_clusters)
