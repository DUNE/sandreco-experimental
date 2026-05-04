#include <geoinfo/geoinfo.hpp>
#include <geoinfo/grain_info.hpp>
#include <common/sand.h>
#include <grain/grain.h>
#include <grain/voxels.h>
#include <grain/point_cloud.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <cmath>

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
   * | `fiducial_distance`  | double    | mm         | Defalut: 0.0      | Minimum distance from fiducial surface.              |
   * | `amp_thr`            | double    | npe        | Defalut: 0.0      | Minimum voxel amplitude.                             |
   * | `voxel_size`         | double    | mm         | Required          | Size of voxels.                                      |
   */


  class voxels_to_point_cloud : public ufw::process {
   public:
   voxels_to_point_cloud();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    double m_fiducial_distance;
    double m_amp_thr;
    double m_voxel_size;
  };

  void voxels_to_point_cloud::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_fiducial_distance = cfg.value("fiducial_distance", 0.0);
    m_amp_thr = cfg.value("amp_thr", 0.0);
    m_voxel_size = cfg.at("voxel_size");
  }


  voxels_to_point_cloud::voxels_to_point_cloud() : process({{"photon_amplitudes", "sand::grain::voxels"}}, {{"point_cloud", "sand::grain::point_cloud"}}) {
    UFW_INFO("Creating a voxels_to_point_cloud process at {}", fmt::ptr(this));
  }

  void voxels_to_point_cloud::run() {
    const auto& gi = instance<geoinfo>();
    const auto& photon_amplitude_in  = get<voxels>("photon_amplitudes");
    auto& point_cloud_out = set<point_cloud>("point_cloud").points; 

    int i_evt{0};

    for (const auto& evt_voxels : photon_amplitude_in.voxels) {
      std::vector<point_cloud::point> evt_points;
      const size_3d sizes = evt_voxels.size();
      const xform_3d transform = evt_voxels.xform_id_to_fiducial(dir_3d(m_voxel_size, m_voxel_size, m_voxel_size));
      for (size_t i{0}; i < sizes.x(); ++i) {
        for (size_t j{0}; j < sizes.y(); ++j) {
          for (size_t k{0}; k < sizes.z(); ++k) {
            index_3d i_voxel(i, j, k);
            double amplitude = evt_voxels.at(i_voxel);
            // Apply threshold on amplitude
            if (amplitude < m_amp_thr) continue;
            pos_3d position = transform * pos_3d(i, j, k);
            dir_3d absolute_position(std::abs(position.x()), std::abs(position.y()), std::abs(position.z()));
            dir_3d displacement_from_wall = gi.grain().fiducial_bbox() - absolute_position;
            // Apply threshold on position
            if (displacement_from_wall.x() < m_fiducial_distance || displacement_from_wall.y() < m_fiducial_distance || displacement_from_wall.z() < m_fiducial_distance) continue;
            evt_points.emplace_back(point_cloud::point{position, amplitude});
            UFW_DEBUG("Index: {}, Position: {}, Amplitude {}", i_voxel, position, amplitude);
          }
        }
      }
      point_cloud_out.emplace_back(evt_points);
      UFW_INFO("Spill {}, event {}, point cloud size: {}", ufw::context::current()->id(), i_evt, evt_points.size());
      i_evt++;
    };
  }

} // namespace sand::grain

UFW_REGISTER_PROCESS(sand::grain::voxels_to_point_cloud)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::voxels_to_point_cloud)
