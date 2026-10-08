#pragma once

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
  struct digits_container : managed_data_base {
    /// @brief Digitized signal with timing and charge information
    ///
    /// A signal extends the base reco::digi class with additional ADC, TDC, and TOT
    /// measurements, providing comprehensive digitization data from the calorimeter.
    struct digit : reco::digi<pes_container::photo_electron> {
      using digi_base_type = reco::digi<pes_container::photo_electron>;
      /// @brief Default constuctor produces an invalid digit, required by ROOT, do not use
      digit() : digi_base_type() {}
      /// @brief Constructor for a simulation digi
      digit(channel_id ch, time t, double a, double tt) : digi_base_type(ch, t, source::sim), m_adc(a), m_tot(tt) {}

      /// @brief Analog-to-digital conversion value representing charge
      double adc() const { return m_adc; };

      /// @brief The TDC time coincides with the best estimate for the digi time
      double tdc() const { return t(); }

      /// @brief Time-over-threshold value for pulse width information
      double tot() const { return m_tot; };

     private:
      double m_adc = NAN;
      double m_tot = NAN;
    };

    /// @brief Vector container for digitized signals
    using digits_collection = std::vector<digit>;

    /// @brief Collection of digitized signals from the calorimeter
    digits_collection digits;
  };
} // namespace sand::ecal

UFW_DECLARE_MANAGED_DATA(sand::ecal::digits_container);
