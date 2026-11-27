#pragma once

#include <common/digi.h>
#include <common/sand.h>
#include <ecal/photo_electron.h>

namespace sand::ecal {
  /// @brief Digitized ECAL signal with timing, charge and photo-electron truth information.
  ///
  /// Extends reco::digi with ADC and TOT measurements; the inherited time is the TDC value.
  struct digit : reco::digi<pe> {
    using digi_base_type = reco::digi<pe>;
    /// @brief Default constructor produces an invalid digit, required by ROOT, do not use
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
} // namespace sand::ecal

// Expose the inherited digi state to ROOT dictionary generation.
UFW_DECLARE_UNMANAGED_DATA(sand::reco::digi<sand::ecal::pe>);
SAND_DATA_COLLECTION(sand::ecal, digit, digits)
