#pragma once

#include <ufw/process.hpp>

namespace sand::ecal {

  class reco_cell_builder : public ufw::process {
   public:
    reco_cell_builder();

    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    /// Conversion from digit ADC units to photo-electrons.
    ///
    /// The current ECal fast digitizer stores the number of collected
    /// photo-electrons in digit::adc(), so the default is 1.
    double m_adc_to_pe = 1.0;

    /// Scintillation light yield used to convert corrected photo-electrons to
    /// deposited energy.
    /// Effective PE/MeV factor used to convert PE counts alreday corrected for fiber attenuation
    /// back to deposited energy. This should match the current optical_simulation
    /// `light_yield`, which is effectively PE/MeV rather than raw photons/MeV.
    double m_light_yield_pe_per_mev = 18.5;

    /// If true, correct the two PMT amplitudes for the cell fiber attenuation
    /// before estimating the deposited energy.
    bool m_apply_attenuation_correction = true;
  };

} // namespace sand::ecal

UFW_REGISTER_PROCESS(sand::ecal::reco_cell_builder)
