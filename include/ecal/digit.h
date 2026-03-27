#pragma once

#include <ufw/data.hpp>
#include <common/digi.h>
#include <common/sand.h>
#include <ecal/photo_electron.h>

namespace sand::ecal {
  /// @brief Digitized signal data container for ECAL
  ///
  /// The digits_container struct represents a managed data container that stores digitized
  /// signal information from the electromagnetic calorimeter detectors. Each signal
  /// contains ADC (analog-to-digital conversion), TDC (time-to-digital conversion),
  /// and TOT (time-over-threshold) measurements.
  struct digits_container : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    /// @brief Digitized signal with timing and charge information
    ///
    /// A signal extends the base reco::digi class with additional ADC, TDC, and TOT
    /// measurements, providing comprehensive digitization data from the calorimeter.
    struct digit : reco::digi<pes_container::photo_electron> {
      using digi_base_type = reco::digi<pes_container::photo_electron>;
      /// @brief Constructor for a simulation digi
      digit(channel_id ch, time t) :
        digi_base_type(ch, t, source::sim), adc(NAN), tot(NAN) {}

      /// @brief Analog-to-digital conversion value representing charge
      double adc;

      /// @brief Time-over-threshold value for pulse width information
      double tot;
    };

    /// @brief Vector container for digitized signals
    using digits_collection = std::vector<digit>;

    /// @brief Collection of digitized signals from the calorimeter
    digits_collection digits;
  };
} // namespace sand::ecal

UFW_DECLARE_MANAGED_DATA(sand::ecal::digits_container);
