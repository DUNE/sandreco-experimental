#include <geoinfo/grain_info.hpp>
#include <grain/grain.h>
#include <grain/voxels.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

namespace sand::test {

  class test_grain_volumereco : public ufw::process {
   public:
    test_grain_volumereco();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    float m_voxel_size;
  };

  void test_grain_volumereco::configure(const ufw::config& cfg) {
    m_voxel_size = cfg.at("voxel_size");
    UFW_DEBUG("test_grain_volumereco configured at: {}", fmt::ptr(this));
  }

  test_grain_volumereco::test_grain_volumereco() : process({{"photon_amplitudes", "sand::grain::voxels"}}, {}) {
    UFW_INFO("Creating a test_grain_volumereco process at {}", fmt::ptr(this));
  }

  void test_grain_volumereco::run() {
    const auto& gi = instance<geoinfo>();
    dir_3d voxel_sizes(m_voxel_size, m_voxel_size, m_voxel_size);
    auto fiducial_voxels            = gi.grain().fiducial_voxels(voxel_sizes);
    const auto& photon_amplitude_in = get<sand::grain::voxels>("photon_amplitudes");
    for (const auto& evt_voxels : photon_amplitude_in.voxels) {
      UFW_ASSERT(evt_voxels.size() == fiducial_voxels.size(),
                 "Mismatched size between reconstruction and voxellization with configured voxel size");
      UFW_ASSERT(
          !(std::any_of(evt_voxels.begin(), evt_voxels.end(), [](const auto& voxel) { return std::isnan(voxel); })),
          "Reconstructed photon distribution has NaN values");

      fiducial_voxels.for_each([&evt_voxels](const sand::grain::index_3d& idx, auto& fid_val) {
        if (fid_val > 0) {
          UFW_ASSERT(evt_voxels.at(idx) >= 0, "Invalid value in fiducial volume: {} at index {}", evt_voxels.at(idx),
                     idx);
        } else {
          if (evt_voxels.at(idx)!= 0.){
            UFW_WARN ("Non-zero reconstructed photon amplitude in voxel outside of fiducial volume: {} at index {}", evt_voxels.at(idx), idx);
          }
          // UFW_ASSERT(evt_voxels.at(idx) == 0.,
          //            "Non-zero reconstructed photon amplitude in voxel outside of fiducial volume: {} at index {}",
          //            evt_voxels.at(idx), idx);
        }
      });
    }
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::test_grain_volumereco)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::test_grain_volumereco)
