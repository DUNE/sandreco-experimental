#include "ufw/utils.hpp"
#include <geoinfo/grain_info.hpp>
#include <hdf5/hdf5.hpp>
#include <grain/grain.h>
#include <grain/voxels.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

namespace sand::test {

  class test_grain_mask_weight_computation : public ufw::process {
   public:
    test_grain_mask_weight_computation();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    float m_voxel_size;
  };

  void test_grain_mask_weight_computation::configure(const ufw::config& cfg) {
    UFW_DEBUG("test_grain_mask_weight_computation configured at: {}", fmt::ptr(this));
    m_voxel_size = cfg.at("voxel_size");
  }

  test_grain_mask_weight_computation::test_grain_mask_weight_computation() : process({}, {}) {
    UFW_INFO("Creating a test_grain_mask_weight_computation process at {}", fmt::ptr(this));
  }

  void test_grain_mask_weight_computation::run() {
    UFW_DEBUG("test_grain_mask_weight_computation run called with context_id: {}", ufw::context::current()->id());
    auto& weights  = instance<sand::hdf5::ndarray>("angle_reader");
    const auto& gi = instance<geoinfo>();
    dir_3d voxel_sizes(m_voxel_size, m_voxel_size, m_voxel_size);
    auto voxels = gi.grain().fiducial_voxels(voxel_sizes);
    const auto& in_weights_file = weights.datasets();
    const auto& in_geometry = gi.grain().mask_cameras();
    //check weights are consistent with geometry
    if (in_weights_file.size() != in_geometry.size()) {
      UFW_ERROR("Mismatch between camera count in geometry ({}) and computed weight arrays ({})",
                in_geometry.size(), in_weights_file.size());
    }
    for (const auto& g_cam : in_geometry) {
      auto it = std::find(in_weights_file.begin(), in_weights_file.end(), g_cam.name);
      if (it == in_weights_file.end()) {
        UFW_ERROR("Camera {} from geometry not found in file.");
      }
      auto ndr = weights.range(g_cam.name);
      if (ndr[0] != voxels.size().x() || ndr[1] != voxels.size().y() ||
          ndr[2] != voxels.size().z() || ndr[3] != g_cam.sipm_active_areas.size()) {
        UFW_ERROR("Camera {} has mismatched sizes: geometry {} x {}, weights file {}",
                  g_cam.name, voxels.size(), g_cam.sipm_active_areas.size(), ndr);
      }
    }
    //check weights have reasonable values
    for (const auto& g_cam : in_geometry) {
      UFW_INFO("camera name {}", g_cam.name);
      auto ws = weights.read<float>(g_cam.name);
      std::size_t total_size = weights.range(g_cam.name).flat_size();
      std::size_t cam_size = g_cam.sipm_active_areas.size();
      const float* base_ptr = ws.get();
      float w_sum = 0.f;
      voxels.for_each([&](auto idx, auto fid_val) {
        const float* cam_ptr = base_ptr + voxels.linear(idx) * cam_size;
        for (std::size_t i = 0; i != cam_size; ++i) {
          float w = cam_ptr[i];
          if (fid_val) {
            w_sum += w;
            UFW_ASSERT(w >= 0.0 && w <= 1.0, "Invalid weight {} in fiducial, at index {}, {}", w, idx, i);
          } else {
            UFW_ASSERT(w == 0.0, "Invalid weight {} outside of fiducial, at index {}, {}", w, idx, i);
          }
        }
      });
      UFW_INFO("camera {} weights sum = {}", g_cam.name, w_sum);
    }
  }
} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::test_grain_mask_weight_computation)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::test_grain_mask_weight_computation)
