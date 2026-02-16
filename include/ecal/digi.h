#pragma once

#include <ufw/data.hpp>
#include <common/digi.h>
#include <common/sand.h>

namespace sand::ecal {
  /// @brief Digitized signal data container for ECAL
  ///
  /// The digi struct represents a managed data container that stores digitized
  /// signal information from the electromagnetic calorimeter detectors. Each signal
  /// contains ADC (analog-to-digital conversion), TDC (time-to-digital conversion),
  /// and TOT (time-over-threshold) measurements.
  struct digi : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    /// @brief Digitized signal with timing and charge information
    ///
    /// A signal extends the base reco::digi class with additional ADC, TDC, and TOT
    /// measurements, providing comprehensive digitization data from the calorimeter.
    struct signal : reco::digi {
        /// @brief Analog-to-digital conversion value representing charge
        double adc;
        
        /// @brief Time-to-digital conversion value representing timing information
        double tdc;
        
        /// @brief Time-over-threshold value for pulse width information
        double tot;
    };

    /// @brief Vector container for digitized signals
    using signal_collection = std::vector<signal>;
    
    /// @brief Collection of digitized signals from the calorimeter
    signal_collection signals;
  };
} // namespace sand::ecal

UFW_DECLARE_MANAGED_DATA(sand::ecal::digi);
