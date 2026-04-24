#include <algorithm>
#include <array>
#include <vector>
#include <iostream>
#include <string>

#include <hdf5/hdf5.hpp>
#include <common/sand.h>
#include <grain/grain.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

namespace sand::grain {

  /**
   * \class sand::grain::mask_voxel_calorimetry
   *
   * \brief brief description
   *
   *    *
   * \subsection Configuration
   * | Parameter Name            | Type             | Unit   | Required/Default                 | Description                                        |
   * |---------------------------|------------------|--------|----------------------------------|----------------------------------------------------|
   * | `slice_times`             | vector\<double\> | ns     | Default: []                      | Predefined time slices for photon assignment.      |
   * | `min_response_signal`     | double           | **??** | Required if slice_times is empty | Minimum photon response signal to trigger slicing. |
   * | `delta_ns_for_comparison` | double           | ns     | Required if slice_times is empty | **??**                                             |
   */

  class mask_voxel_calorimetry : public ufw::process {
   public:
   mask_voxel_calorimetry();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    double m_calib_A;
    double m_calib_B;

  };

  void mask_voxel_calorimetry::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_calib_A = cfg.at("calib_A");
    m_calib_B = cfg.at("calib_B");
  }


  mask_voxel_calorimetry::mask_voxel_calorimetry() : process({}, {}) {
    UFW_INFO("Creating a mask_voxel_calorimetry process at {}", fmt::ptr(this));
  }

  void mask_voxel_calorimetry::run() {
    auto& recos = instance<sand::hdf5::ndarray>("reco_reader");
    auto r = recos.datasets();
    auto current_id = static_cast<uint64_t>(ufw::context::current()->id());
    auto current_id_s = std::to_string(current_id);
    std::cout << "current id " << current_id << ", r[id] " << r[current_id] << std::endl;
    
  }

} // namespace sand::grain

UFW_REGISTER_PROCESS(sand::grain::mask_voxel_calorimetry)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::mask_voxel_calorimetry)
