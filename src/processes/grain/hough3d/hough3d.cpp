#include <common/sand.h>
#include <geoinfo/grain_info.hpp>
#include <grain/point_cloud.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <ocl/ocl.hpp>
#include <hough3d.hpp>

namespace sand::grain {

  /**
   * \class sand::grain::hough3d
   *
   * \brief Uses a 3D Hough transform to perform clustering and tracking in a 3D point cloud.
   *
   *
   * \subsection Configuration
   * | Parameter Name           | Type   | Unit            | Required/Default | Description                                                                         |
   * |--------------------------|--------|-----------------|------------------|-------------------------------------------------------------------------------------|
   * | `n_versors_in_sphere`    | uint   |                 | Default: 100     | How many versors represent Fibonacci's sphere, for direction binning.               |
   * | `xy_plane_step`          | double | mm              | Required         | Bin size for xy plane in Hough space.                                               |
   * | `min_points_per_track`   | uint   |                 | Required         | Minimum number of points to build a track.                                          |
   * | `max_tracks_per_event`   | uint   |                 | Default: 4       | Maximum number of tracks to search for in one event.                                |
   */
  class hough3d : public ufw::process {
   public:
    hough3d();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    static constexpr size_t s_max_platforms = 4;
    uint m_n_versors_in_sphere;
    double m_xy_plane_step;
    uint m_min_points_per_track;
    uint m_max_tracks_per_event;
    std::vector<dir_3d> m_unique_versors;
  };

  void hough3d::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_n_versors_in_sphere = cfg.value("n_versors_in_sphere", 100);
    m_xy_plane_step = cfg.at("xy_plane_step");
    m_min_points_per_track = cfg.at("min_points_per_track");
    m_max_tracks_per_event = cfg.value("max_tracks_per_event", 4);

    // Binning versors and x'y' plane
    const std::vector<dir_3d> versors = fibonacci_sphere_versors(m_n_versors_in_sphere);
    m_unique_versors = select_unique_versors(versors);

    const auto& gi = ufw::context::current()->instance<geoinfo>();
    const dir_3d grain_dimensions = gi.grain().fiducial_bbox();
    const double xy_half_range = grain_dimensions.R();
  }

  hough3d::hough3d() : process({{"point_cloud", "sand::grain::point_cloud"}}, {}) {
    UFW_DEBUG("Creating a hough3d process at {}.", fmt::ptr(this));
  }

  void hough3d::run() {
    UFW_DEBUG("Running a hough3d process at {}.", fmt::ptr(this));
    auto& platform = instance<cl::platform>();
    const auto& point_cloud_in  = get<point_cloud>("point_cloud");

  }
} // namespace sand::grain

UFW_REGISTER_PROCESS(sand::grain::hough3d)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::hough3d)
