#include <algorithm>
#include <array>
#include <vector>
#include <iostream>
#include <string>

#include <hdf5/hdf5.hpp>
#include <common/sand.h>
#include <grain/grain.h>
#include <grain/voxels.h>

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
    double m_calib_m;
    double m_calib_q;
    uint64_t m_stat_events_processed;
  };

  void mask_voxel_calorimetry::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_calib_m = cfg.at("calib_slope");
    m_calib_q = cfg.at("calib_intercept");
  }


  mask_voxel_calorimetry::mask_voxel_calorimetry() : process({{"photon_amplitudes", "sand::grain::voxels"}}, {}) {
    UFW_INFO("Creating a mask_voxel_calorimetry process at {}", fmt::ptr(this));
  }

  void mask_voxel_calorimetry::run() {
    const auto& photon_amplitude_in      = get<voxels>("photon_amplitudes");
    UFW_DEBUG("Reconstructed {} events in spill", photon_amplitude_in.voxels.size());
    m_stat_events_processed = 0.;
    for (const auto& evt_voxels : photon_amplitude_in.voxels) {
      double total_amplitude = std::accumulate(evt_voxels.begin(), evt_voxels.end(), 0.);
      m_stat_events_processed++;
      double deposited_energy = m_calib_m * total_amplitude + m_calib_q;
      UFW_DEBUG("Event {}: photon amplitude total = {}, deposited energy = {} MeV", m_stat_events_processed, total_amplitude, deposited_energy);
    };
  }

} // namespace sand::grain

UFW_REGISTER_PROCESS(sand::grain::mask_voxel_calorimetry)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::mask_voxel_calorimetry)
