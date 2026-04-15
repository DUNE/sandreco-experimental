#include <fast_digi.hpp>
#include <geoinfo/ecal_info.hpp>
#include <ecal/digit.h>
#include <ecal/photo_electron.h>

#include <ufw/factory.hpp>

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
   * | Parameter Name      | Type     | Unit            | Required/Default | Description                                                   |
   * |---------------------|----------|-----------------|------------------|---------------------------------------------------------------|
   * | `int_time_window`   | `double` | nanoseconds     | Required         | Integration window duration for accumulating photo-electrons. |
   * | `dead_time_window`  | `double` | nanoseconds     | Required         | Electronics dead time after pulse detection.                  |
   * | `pe_threshold`      | `double` | photo-electrons | Required         | Minimum photo-electrons required to trigger a pulse output.   |
   * | `constant_fraction` | `double` | ratio [0.0-1.0] | Required         | CFD Threshold Fraction.                                       |
   * 
   */

  /// Configure digitization parameters from configuration file
  void fast_digi::configure(const ufw::config& cfg) {
    process::configure(cfg);
    // Time window for integrating photo-electrons into a single pulse
    m_int_time_window = cfg.at("int_time_window");
    // Dead time window during which no new pulses can be detected
    m_dead_time_window = cfg.at("dead_time_window");
    // Minimum number of photo-electrons required to trigger a digitized signal
    m_pe_threshold = cfg.at("pe_threshold");
    // Constant fraction for timing discrimination (constant fraction discriminator)
    m_constant_fraction = cfg.at("constant_fraction");
  }

  /// Constructor: Initialize digitization process with PES input and DIGI output
  fast_digi::fast_digi() : process({{"pes", "sand::ecal::pes_container"}}, {{"digi", "sand::ecal::digits_container"}}) {
    UFW_DEBUG("Creating a ecal fast digitization process at {}", fmt::ptr(this));
  }

  /// Main digitization loop: Convert photo-electrons to digitized signals
  /// Implements a sliding time window algorithm with dead time and threshold detection
  void fast_digi::run() {
    UFW_DEBUG("Running a ecal fast digitization process at {}", fmt::ptr(this));
    // Get ECAL geometry information
    const auto& gecal = get<geoinfo>().ecal();
    // Get input photo-electron collection
    auto& pes = get<sand::ecal::pes_container>("pes");
    // Get output digitized signal collection
    auto& digi = set<sand::ecal::digits_container>("digi");

    // Process photo-electrons for each PMT channel
    for (auto [pmt, pe_collection] : pes.collection) {
      // Sort photo-electrons by arrival time for temporal processing
      std::sort(pe_collection.begin(), pe_collection.end(),
                [](const auto& a, const auto& b) { return a.arrival_time < b.arrival_time; });
      // Initialize sliding window starting with first photo-electron
      auto start_pe         = pe_collection.begin();
      auto start_int_window = start_pe->arrival_time;
      auto this_pe          = start_pe;

      // Sliding window loop: collect PEs within integration window
      while (true) {
        // Find all photo-electrons within the integration time window
        while (this_pe->arrival_time < start_int_window + m_int_time_window && this_pe != pe_collection.end()) {
          this_pe++;
        }
        // Count photo-electrons in current window
        auto pe_count = std::distance(start_pe, this_pe) + 1; // +1 to include the boundary PE

        // Check if pulse meets minimum threshold for digitization
        if (pe_count >= m_pe_threshold) {
          // Calculate timing using constant fraction discriminator method
          auto tdc = std::next(start_pe, int(m_constant_fraction * pe_count))->arrival_time;

          // Create digitized signal with PMT channel, timing window, and measurements
          digits_container::digit signal(pmt,
          // timing window for particle crossing is conservatively estimated taking into
          // account a maximal path length for scintillation photons of 5 m, a velocity of
          // 5.85 ns/m and a scintillation time of 3.08 ns, which gives a total of about 35 ns.
                                         {tdc - 35., tdc, tdc + 5.},
          // Calculate ADC value proportional to collected photo-electrons
          // for now, we just use the number of PEs as the ADC value.
          // This can be improved by using a more realistic response function.
                                         static_cast<double>(pe_count),
          // We don't have a good way to estimate the TOT value, so we set it to NAN for now. This can
          // be improved by using a more realistic response function that includes the pulse shape.
                                         NAN);
          // Collect all truth hits from photo-electrons in this pulse
          auto it = start_pe;

          // Add truth hit information from all contributing photo-electrons
          while (it != this_pe) {
            signal.insert(*it);
            it++;
          }
          // Include the boundary photo-electron if it exists
          if (this_pe != pe_collection.end())
            signal.insert(*this_pe);

          // Store the digitized signal in output collection
          digi.digits.push_back(signal);

          // Skip photo-electrons in the dead time window after signal detection
          while (this_pe->arrival_time < start_int_window + m_int_time_window + m_dead_time_window
                 && this_pe != pe_collection.end()) {
            this_pe++;
          }
          // Check if we've processed all photo-electrons
          if (this_pe == pe_collection.end())
            break;
          // Restart search from after dead time
          start_pe = std::next(this_pe);
        } else {
          // Pulse below threshold: advance starting point and continue searching
          if (start_pe == pe_collection.end())
            break;
          start_pe = std::next(start_pe);
        }
        // Update integration window with next starting photo-electron
        start_int_window = start_pe->arrival_time;
        this_pe          = start_pe;
      }
    }
  }
} // namespace sand::ecal
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::ecal::fast_digi)
