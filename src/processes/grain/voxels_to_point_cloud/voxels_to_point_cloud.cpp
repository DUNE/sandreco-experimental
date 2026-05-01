#include <common/sand.h>
#include <grain/grain.h>
#include <grain/voxels.h>
#include <grain/point_cloud.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

namespace sand::grain {

  /**
   * \class sand::grain::voxels_to_point_cloud
   *
   * \brief Converts voxel amplitudes into a point cloud
   *
   * This process reads photon amplitude voxel arrays, applies a geometrical and amplitued threshold,
   * and converts it to a 3D point cloud.
   *
   * \subsection Input
   * - **photon_amplitudes** (`sand::grain::voxels`): Voxelized photon amplitude data organized by event.
   *   Each event contains a voxel_array of amplitude values across the detector.
   *
   * \subsection Configuration
   * | Parameter Name       | Type      | Unit       | Required/Default  | Description                                          |
   * |----------------------|-----------|------------|-------------------|------------------------------------------------------|
   * | `dist_wall`          | double    | cm         | Defalut: 0.0      | Minimum distance from fiducial surface.              |
   * | `amp_thr`            | double    | npe        | Defalut: 0.0      | Minimum voxel amplitude.                             |
   */


  class voxels_to_point_cloud : public ufw::process {
   public:
   voxels_to_point_cloud();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    double m_dist_wall;
    double m_amp_thr;
  };

  void voxels_to_point_cloud::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_dist_wall = cfg.value("dist_wall", 0.0);
    m_amp_thr = cfg.value("amp_thr", 0.0);
  }


  voxels_to_point_cloud::voxels_to_point_cloud() : process({{"photon_amplitudes", "sand::grain::voxels"}}, {{"point_cloud", "sand::grain::point_cloud"}}) {
    UFW_INFO("Creating a voxels_to_point_cloud process at {}", fmt::ptr(this));
  }

  void voxels_to_point_cloud::run() {
    const auto& photon_amplitude_in  = get<voxels>("photon_amplitudes");
    auto& point_cloud_out = set<point_cloud>("point_cloud").points; 
    for (const auto& evt_voxels : photon_amplitude_in.voxels) {
      std::vector<point_cloud::point> evt_points;
      const size_3d sizes = evt_voxels.size();
      const xform_3d transform = evt_voxels.xform_id_to_fiducial(dir_3d(150.0, 150.0, 150.0));
      for (size_t i{0}; i < sizes.x(); ++i) {
        for (size_t j{0}; j < sizes.y(); ++j) {
          for (size_t k{0}; k < sizes.z(); ++k) {
            index_3d i_voxel(i, j, k);
            double amplitude = evt_voxels.at(i_voxel);
            if (amplitude < m_amp_thr) continue;
            pos_3d position = transform * pos_3d(i, j, k);
            evt_points.emplace_back(point_cloud::point{position, amplitude});
            UFW_DEBUG("Index: {}, Position: {}, Amplitude {}", i_voxel, position, amplitude);
          }
        }
      }
      point_cloud_out.emplace_back(evt_points);
    };
  }

} // namespace sand::grain

UFW_REGISTER_PROCESS(sand::grain::voxels_to_point_cloud)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::voxels_to_point_cloud)
