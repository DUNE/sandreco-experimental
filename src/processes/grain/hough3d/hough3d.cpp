#include <common/sand.h>
#include <geoinfo/grain_info.hpp>
#include <grain/point_cloud.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <ocl/ocl.hpp>
#include <hough3d.hpp>

#include <cmath>

namespace sand::grain {

  /**
   * \class sand::grain::hough3d
   *
   * \brief Uses a 3D Hough transform to perform clustering and tracking in a 3D point cloud.
   *
   *
   * \subsection Configuration
   * | Parameter Name           | Type   | Unit  | Required/Default | Description                                                                         |
   * |--------------------------|--------|-------|------------------|-------------------------------------------------------------------------------------|
   * | `n_versors_in_sphere`    | uint   |       | Default: 100     | How many versors represent Fibonacci's sphere, for direction binning.               |
   * | `xy_plane_step`          | double | mm    | Required         | Bin size for xy plane in Hough space.                                               |
   * | `min_points_per_track`   | uint   |       | Required         | Minimum number of points to build a track.                                          |
   * | `max_tracks_per_event`   | uint   |       | Default: 4       | Maximum number of tracks to search for in one event.                                |
   */
  class hough3d : public ufw::process {
   public:
    hough3d();
    void configure(const ufw::config& cfg) override;
    void configure_hough_vote(cl::platform& platform);
    void run() override;

   private:
    static constexpr size_t s_max_platforms = 4;
    uint m_n_versors_in_sphere;
    double m_xy_plane_step;
    size_t m_n_xy_bins;
    uint m_min_points_per_track;
    uint m_max_tracks_per_event;
    std::vector<cl_float3> m_unique_versors;
    std::vector<float> m_voting_array;
    cl::Program m_hough_vote_program;
    cl::Kernel m_hough_vote_kernel;
  };

  void hough3d::configure_hough_vote(cl::platform& platform) {
    const char* code_kernel_src =
#include "cl_src/hough_vote.cl"
        ;
    platform.build_program(m_hough_vote_program, code_kernel_src);
    m_hough_vote_kernel = cl::Kernel(m_hough_vote_program, "hough_vote");
  }

  void hough3d::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_n_versors_in_sphere = cfg.value("n_versors_in_sphere", 100);
    m_xy_plane_step = cfg.at("xy_plane_step");
    m_min_points_per_track = cfg.at("min_points_per_track");
    m_max_tracks_per_event = cfg.value("max_tracks_per_event", 4);

    // Binning versors and x'y' plane
    const std::vector<cl_float3> versors = fibonacci_sphere_versors(m_n_versors_in_sphere);
    m_unique_versors = select_unique_versors(versors);
    const auto& gi = ufw::context::current()->instance<geoinfo>();
    const dir_3d grain_dimensions = gi.grain().fiducial_bbox();
    const double xy_half_range = grain_dimensions.R();
    m_n_xy_bins = static_cast<size_t>(std::ceil(2.0 * xy_half_range / m_xy_plane_step));

    m_voting_array.assign(m_unique_versors.size() * m_n_xy_bins * m_n_xy_bins, 0.0);
    UFW_INFO("Size of voting array: {}", m_voting_array.size());
    UFW_INFO("Memory size of voting array: {} kB", static_cast<float>(sizeof(m_voting_array)) / 1024.0);

    auto& platform = instance<cl::platform>();
    configure_hough_vote(platform);
  }

  hough3d::hough3d() : process({{"point_cloud", "sand::grain::point_cloud"}}, {}) {
    UFW_DEBUG("Creating a hough3d process at {}.", fmt::ptr(this));
  }

  void hough3d::run() {
    UFW_DEBUG("Running a hough3d process at {}.", fmt::ptr(this));
    auto& platform = instance<cl::platform>();
    const auto& point_cloud_in  = get<point_cloud>("point_cloud");
    // Loop on events in a spill
    for (const auto& ev_points : point_cloud_in.points) {
      std::vector<cl_float3> cl_points = point_cloud_to_float3(ev_points);
      UFW_INFO("Processing {} points", cl_points.size());
      cl::buffer buf_points;
      buf_points.allocate<CL_MEM_COPY_HOST_PTR | CL_MEM_READ_WRITE>(
          platform.context(), cl_points.size() * sizeof(cl_float4), cl_points.data());
    }

  }
} // namespace sand::grain

UFW_REGISTER_PROCESS(sand::grain::hough3d)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::hough3d)
