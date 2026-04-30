#include <common/sand.h>
#include <grain/grain.h>
#include <grain/voxels.h>
#include <grain/energy.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

namespace sand::grain {

  /**
   * \class sand::grain::mask_voxel_calorimetry
   *
   * \brief Reconstructs deposited energy from voxelized photon amplitude data using calibration coefficients.
   *
   * This process reads photon amplitude voxel arrays, computes the total amplitude per event,
   * and converts it to deposited energy using a linear calibration model. The calibration
   * parameters map raw photon amplitudes to reconstructed energy values.
   *
   * \subsection Input
   * - **photon_amplitudes** (`sand::grain::voxels`): Voxelized photon amplitude data organized by event.
   *   Each event contains a voxel_array of amplitude values across the detector.
   *
   * \subsection Configuration
   * | Parameter Name       | Type      | Unit       | Required/Default  | Description                                          |
   * |----------------------|-----------|------------|-------------------|------------------------------------------------------|
   * | `calib_slope`        | double    | MeV/npe    | Required          | Calibration slope (m) for energy reconstruction.     |
   * | `calib_intercept`    | double    | MeV        | Required          | Calibration intercept (q) for energy reconstruction. |
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


  mask_voxel_calorimetry::mask_voxel_calorimetry() : process({{"photon_amplitudes", "sand::grain::voxels"}}, {{"total_deposited_energy", "sand::grain::total_energy"}}) {
    UFW_INFO("Creating a mask_voxel_calorimetry process at {}", fmt::ptr(this));
  }

  void mask_voxel_calorimetry::run() {
    const auto& photon_amplitude_in  = get<voxels>("photon_amplitudes");
    auto& total_deposited_energy_out = set<total_energy>("total_deposited_energy").energies; 
    m_stat_events_processed = 0.;
    for (const auto& evt_voxels : photon_amplitude_in.voxels) {
      double total_amplitude = std::accumulate(evt_voxels.begin(), evt_voxels.end(), 0.);
      total_deposited_energy_out.emplace_back( m_calib_m * total_amplitude + m_calib_q);
      m_stat_events_processed++;
    };
    UFW_DEBUG("Processed {} events in spill", m_stat_events_processed);
  }

} // namespace sand::grain

UFW_REGISTER_PROCESS(sand::grain::mask_voxel_calorimetry)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::mask_voxel_calorimetry)
