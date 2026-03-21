#include <ufw/factory.hpp>
#include <ufw/process.hpp>

namespace sand::ecal {
/**
 * \class sand::ecal::fast_digi
 * \brief Processes raw photo-electron (PE) inputs from PMTs to digitized signals,
 *        leveraging a sliding-window algorithm to detect pulses.
 *
 * This process utilizes a sliding time window to integrate PMT photo-electrons,
 * enforcing dead-time to avoid pile-up artifacts, and rejecting small pulse
 * clusters below a photo-electron threshold. The output generates digitized
 * event containers suitable for real-time or offline analysis pipelines.
 *
 * \details The `fast_digi` processor converts photo-electron events originating
 *          from Photon Detection Tubes (PMT) into time-based digitized pulse signals.
 *          It applies constant fraction timing discrimination during pulse identification,
 *          optimizing event reconstruction for high-rate PMT data.
 *
 * \configuration Confirms the process with the following PMT photo-electron-related parameters:
 * - \c int_time_window: Time window for PE clustering integration [default unset]
 * - \c dead_time_window: Deadtime threshold for rejecting overlapping events [default unset]
 * - \c pe_threshold: Minimum photo-electron count required to produce a digit [default unset]
 * - \c costant_fraction: Fractional threshold for pulse timing discrimination [default unset]
 */
  class fast_digi : public ufw::process {
   public:
    fast_digi();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    /// @brief Integration time window for signal accumulation
    double m_int_time_window;

    /// @brief Dead time window preventing pulse pile-up detection
    double m_dead_time_window;

    /// @brief Threshold for photo-electron detection
    double m_pe_threshold;

    /// @brief Constant fraction for timing discrimination
    double m_costant_fraction;
  };
} // namespace sand::ecal

UFW_REGISTER_PROCESS(sand::ecal::fast_digi)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::ecal::fast_digi)
