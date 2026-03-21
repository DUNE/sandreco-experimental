#include <ufw/factory.hpp>
#include <ufw/process.hpp>

namespace sand::ecal {
  /**
   * \class sand::ecal::fast_digi
   * \brief Processes raw photo-electron (PE) inputs from PMTs to digitized signals,
   *        leveraging a sliding-window algorithm to detect pulses.
   *
   * This process utilizes a sliding time window to integrate PMT photo-electrons,
   * enforcing dead-time to simulate the behaviour of the readout electronics, and rejecting
   * signals below a photo-electron threshold.
   * It applies constant fraction discrimination to extract the best timing of the signal.
   *
   * \subsection Configuration
   * 
   * | Parameter Name          | Type    | Unit               | Required/Default | Description
   * |-------------------------|---------|---------------------|----------|----------------
   * | `int_time_window`      | `double`| nanoseconds       | **Required** | Integration window duration for accumulating photo-electrons.
   * | `dead_time_window`     | `double`| nanoseconds       | **Required** | Electronics dead time after pulse detection.
   * | `pe_threshold`         | `double`| photo-electrons    | **Required** | Minimum photo-electrons required to trigger a pulse output.
   * | `constant_fraction`    | `double`| ratio [0.0-1.0]   | **Required** | CFD Threshold Fraction.
   * 
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
  double m_constant_fraction;
};
} // namespace sand::ecal

UFW_REGISTER_PROCESS(sand::ecal::fast_digi)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::ecal::fast_digi)
