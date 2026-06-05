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
    auto voxels                   = gi.grain().fiducial_voxels(voxel_sizes);
    const auto weights_dimensions = weights.range(weights.datasets().front());
    const size_t weights_size =
        weights_dimensions[0] * weights_dimensions[1] * weights_dimensions[2] * weights_dimensions[3];
    UFW_DEBUG("weights shape: {}, {}, {}, {}", weights_dimensions[0], weights_dimensions[1], weights_dimensions[2],
             weights_dimensions[3]);
    if (weights.datasets().size() != gi.grain().mask_cameras().size()) {
      UFW_ERROR("Mismatch between cameras in geometry ({}) and computed weight arrays ({})",
                gi.grain().mask_cameras().size(), weights.datasets().size());
    }
    if (weights_dimensions[0] != voxels.size().x() || weights_dimensions[1] != voxels.size().y()
        || weights_dimensions[2] != voxels.size().z()) {
      UFW_ERROR("hdf5 voxels shape: ({}, {}, {}). Geometry voxels shape: {}.", weights_dimensions[0],
                weights_dimensions[1], weights_dimensions[2], voxels.size());
    }

    for (const auto& camera : weights.datasets()) {
      grain::voxel_array<grain::pixel_array<float>> camera_weights(voxels.size());
      UFW_ASSERT(sizeof(grain::pixel_array<float>) == 4096, "pixel array is not dense");
      weights.read(camera, camera_weights.data());
      UFW_DEBUG("camera name {}", camera);
      voxels.for_each([&camera_weights](const sand::grain::index_3d idx, auto fid_val) {
        if (fid_val > 0) {
          for (float w : camera_weights.at(idx)) {
            if (w < 0 || w >= 1) {
              UFW_WARN("Invalid weight {} in fiducial, at index {}", w, idx);
            }
          }
        } else {
          for (float w : camera_weights.at(idx)) {
            if (w != 0) {
              UFW_WARN("Invalid weight {} outside of fiducial, at index {}", w, idx);
            }
          }
        }
      });
    }
  }
} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::test_grain_mask_weight_computation)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::test_grain_mask_weight_computation)
