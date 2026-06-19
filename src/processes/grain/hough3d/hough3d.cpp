#include <common/sand.h>
#include <geoinfo/grain_info.hpp>
#include <grain/point_cloud.h>
#include <grain/point_clusters.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <ocl/ocl.hpp>
#include <hough3d.hpp>

#include <cmath>
#include <iostream>

namespace sand::grain {

  /**
   * \class sand::grain::hough3d
   *
   * \brief Uses a 3D Hough transform to perform clustering and tracking in a 3D point cloud.
   *
   *
   * \subsection Configuration
   * | Parameter Name                | Type   | Unit  | Required/Default | Description                                                                         |
   * |-------------------------------|--------|-------|------------------|-------------------------------------------------------------------------------------|
   * | `n_versors_in_sphere`         | uint   |       | Default: 100     | How many versors represent Fibonacci's sphere, for direction binning.               |
   * | `xy_plane_step`               | float  | mm    | Required         | Bin size for xy plane in Hough space.                                               |
   * | `max_clustering_distance`     | float  | mm    | Required         | Max distance between Hough line and points to be clustered together.                |
   * | `min_points_per_track`        | uint   |       | Required         | Minimum number of points to build a track.                                          |
   * | `max_tracks_per_event`        | uint   |       | Default: 4       | Maximum number of tracks to search for in one event.                                |
   */
  class hough3d : public ufw::process {
   public:
    hough3d();
    void configure(const ufw::config& cfg) override;
    void configure_hough_vote(cl::platform& platform);
    void configure_distance(cl::platform& platform);
    void run() override;

   private:
    static constexpr size_t s_max_platforms = 4;
    uint m_n_versors_in_sphere;
    float m_xy_plane_step;
    float m_max_clustering_distance;
    size_t m_n_xy_bins;
    uint m_min_points_per_track;
    uint m_max_tracks_per_event;
    std::vector<cl_float4> m_unique_versors;
    std::vector<uint> m_voting_array;
    cl::Program m_hough_vote_program;
    cl::Kernel m_hough_vote_kernel;
    cl::Program m_distance_program;
    cl::Kernel m_distance_kernel;
    cl::buffer m_buf_unique_versors;
    cl::buffer m_buf_voting_array;
  };

  void hough3d::configure_hough_vote(cl::platform& platform) {
    const char* code_kernel_src =
#include "cl_src/hough_vote.cl"
        ;
    platform.build_program(m_hough_vote_program, code_kernel_src);
    m_hough_vote_kernel = cl::Kernel(m_hough_vote_program, "hough_vote");
  }

  void hough3d::configure_distance(cl::platform& platform) {
    const char* code_kernel_src =
#include "cl_src/distance.cl"
        ;
    platform.build_program(m_distance_program, code_kernel_src);
    m_distance_kernel = cl::Kernel(m_distance_program, "hough_distance");
  }

  void hough3d::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_n_versors_in_sphere = cfg.value("n_versors_in_sphere", 100);
    m_xy_plane_step = cfg.at("xy_plane_step");
    m_max_clustering_distance = cfg.at("max_clustering_distance");
    m_min_points_per_track = cfg.at("min_points_per_track");
    m_max_tracks_per_event = cfg.value("max_tracks_per_event", 4);

    // Binning versors and x'y' plane
    const std::vector<cl_float4> versors = fibonacci_sphere_versors(m_n_versors_in_sphere);
    m_unique_versors = select_unique_versors(versors);
    const auto& gi = ufw::context::current()->instance<geoinfo>();
    const dir_3d grain_dimensions = gi.grain().fiducial_bbox();
    const double xy_half_range = grain_dimensions.R();
    m_n_xy_bins = static_cast<size_t>(std::ceil(2.0 * xy_half_range / m_xy_plane_step));

    m_voting_array.assign(m_unique_versors.size() * m_n_xy_bins * m_n_xy_bins, 0);
    UFW_INFO("Size of voting array: {}", m_voting_array.size());
    UFW_INFO("Memory size of voting array: {} kB", static_cast<float>(sizeof(uint) * m_voting_array.size()) / 1024.0);

    auto& platform = instance<cl::platform>();
    configure_hough_vote(platform);
    configure_distance(platform);

    m_buf_unique_versors.allocate<CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY>(platform.context(), m_unique_versors.size() * sizeof(cl_float4), m_unique_versors.data());
    m_buf_voting_array.allocate<CL_MEM_READ_WRITE>(platform.context(), m_voting_array.size() * sizeof(uint));
  }

  hough3d::hough3d() : process({{"point_cloud", "sand::grain::point_cloud"}}, {{"point_clusters", "sand::grain::point_clusters"}}) {
    UFW_DEBUG("Creating a hough3d process at {}.", fmt::ptr(this));
  }

  void hough3d::run() {
    UFW_DEBUG("Running a hough3d process at {}.", fmt::ptr(this));
    auto& platform = instance<cl::platform>();
    const auto& point_cloud_in  = get<point_cloud>("point_cloud");
    auto& point_clusters_out = set<point_clusters>("point_clusters").clusters;
    // Loop on events in a spill
    // size_t fake_index{500};
    for (const auto& ev_points : point_cloud_in.points) {
      // UFW_INFO("Faking versor index {}, coords ({},{},{})", fake_index, m_unique_versors[fake_index].s[0], m_unique_versors[fake_index].s[1], m_unique_versors[fake_index].s[2]);
      // std::vector<point_cloud::point> fake_points;
      // for (size_t i{0}; i < 20; ++i) {
      //   fake_points.emplace_back(point_cloud::point{{0.0 + 15*i*m_unique_versors[fake_index].s[0], 0.0 + 15*i*m_unique_versors[fake_index].s[1], 0.0 + 15*i*m_unique_versors[fake_index].s[2]}, 0.0});
      // }
      if (ev_points.size() == 0) {
        UFW_DEBUG("Skipping event with 0 points");
        continue;
      }
      for (const auto& p : ev_points) {
        std::cout << p.position.x() << "," << p.position.y() << "," << p.position.z() << "\n";
      }
      std::vector<cl_float4> cl_points = point_cloud_to_float4(ev_points);
      UFW_INFO("Processing {} points", cl_points.size());
      std::vector<point_clusters::cluster> ev_clusters_out;
      uint n_found_tracks{0};
      while (true) {
        cl::buffer buf_points;
        buf_points.allocate<CL_MEM_COPY_HOST_PTR | CL_MEM_READ_WRITE>(platform.context(), cl_points.size() * sizeof(cl_float4), cl_points.data());

        // Reset voting array
        std::fill(m_voting_array.begin(), m_voting_array.end(), 0);
        m_buf_voting_array.write(m_voting_array.data(), platform.queues().front());

        platform.queues().front().finish();

        // set kernel args
        try {
          m_hough_vote_kernel.setArg(0, buf_points);
          m_hough_vote_kernel.setArg(1, m_buf_unique_versors);
          m_hough_vote_kernel.setArg(2, m_buf_voting_array);
          m_hough_vote_kernel.setArg(3, m_xy_plane_step);
          m_hough_vote_kernel.setArg(4, static_cast<int>(m_n_xy_bins));
        } catch (const cl::Error& e) {
          UFW_WARN("OpenCL hough vote Program Kernel setArg: {} ({})", e.what(), e.err());
          throw;
        }

        platform.queues().front().enqueueNDRangeKernel(m_hough_vote_kernel, cl::NullRange, cl::NDRange(cl_points.size(), m_unique_versors.size()), cl::NullRange, nullptr, nullptr);
        platform.queues().front().finish();

        void* tmp_voting_ptr = m_voting_array.data();
        m_buf_voting_array.read(tmp_voting_ptr, platform.queues().front(), 0, -1, {});
        platform.queues().front().finish();

        // Find maximum
        const size_t max_votes_index =  std::distance(m_voting_array.begin(),  std::max_element(m_voting_array.begin(), m_voting_array.end()));
        const uint vote_count = m_voting_array[max_votes_index];
        const size_t max_versor_index = max_votes_index / (m_n_xy_bins * m_n_xy_bins);
        const size_t remainder = max_votes_index % (m_n_xy_bins * m_n_xy_bins);
        const size_t max_x_index = remainder / m_n_xy_bins;
        const size_t max_y_index = remainder % m_n_xy_bins;

        const auto& max_versor = m_unique_versors[max_versor_index];
        const float max_x = (0.5 + max_x_index - m_n_xy_bins / 2) * m_xy_plane_step;
        const float max_y = (0.5 + max_y_index - m_n_xy_bins / 2) * m_xy_plane_step;
        const cl_float3 line_point{max_x, max_y, 0.0};

        UFW_DEBUG("Index of voting array maximum: {}, votes: {}, (versor,x,y): (({},{},{}),{},{})", max_votes_index, vote_count, max_versor.s[0], max_versor.s[1], max_versor.s[2], max_x, max_y);

        // Find points close to line
        cl::buffer buf_distances;
        buf_distances.allocate<CL_MEM_READ_WRITE>(platform.context(), cl_points.size() * sizeof(float));
        // set kernel args
        try {
          m_distance_kernel.setArg(0, buf_points);
          m_distance_kernel.setArg(1, max_versor);
          m_distance_kernel.setArg(2, line_point);
          m_distance_kernel.setArg(3, buf_distances);
        } catch (const cl::Error& e) {
          UFW_WARN("OpenCL distance Program Kernel setArg: {} ({})", e.what(), e.err());
          throw;
        }

        platform.queues().front().enqueueNDRangeKernel(m_distance_kernel, cl::NullRange, cl::NDRange(cl_points.size()), cl::NullRange, nullptr, nullptr);
        platform.queues().front().finish();

        std::vector<float> points_distances(cl_points.size(), 0.0);
        void* tmp_distances_ptr = points_distances.data();
        buf_distances.read(tmp_distances_ptr, platform.queues().front(), 0, -1, {});
        platform.queues().front().finish();

        std::vector<cl_float4> clustered_points = filter_neighbouring_points(cl_points, points_distances, m_max_clustering_distance);

        if (clustered_points.size() < m_min_points_per_track) {
          break;
        }

        const pos_3d out_line_point{line_point.s[0], line_point.s[1], line_point.s[2]};
        const dir_3d out_line_dir{max_versor.s[0], max_versor.s[1], max_versor.s[2]};
        ev_clusters_out.emplace_back(point_clusters::cluster{point_float4_to_cloud(clustered_points), out_line_dir, out_line_point});
        n_found_tracks++;

        UFW_INFO("Added track: point {} direction {} n_points {}", out_line_point, out_line_dir, clustered_points.size());

        if (n_found_tracks >= m_max_tracks_per_event || cl_points.size() == 0) {
          break;
        }

      }
      UFW_INFO("Found {} tracks", n_found_tracks);
    }

  }
} // namespace sand::grain

UFW_REGISTER_PROCESS(sand::grain::hough3d)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::hough3d)
