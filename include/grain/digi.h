#pragma once

#include <vector>

#include <common/digi.h>
#include <common/sand.h>
#include <grain/grain.h>
#include <grain/photons.h>

namespace sand::grain {

  struct digi : managed_data_base {
    struct signal : public reco::digi<photon> {
      using digi_base_type = reco::digi<photon>;
      /// @brief Default constuctor produces an invalid digit, required by ROOT, do not use
      signal() : digi_base_type() {}
      /// @brief Constructor for a simulation digi
      signal(photon truth, channel_id ch, time t, double np, double tt)
        : digi_base_type(truth, ch, t), m_npe(np), m_tot(tt) {}

      /// @brief Amplitude in detected photons
      double npe() const { return m_npe; };

      /// @brief The TDC time coincides with the best estimate for the digi time
      double tdc() const { return t(); }

      /// @brief Time-over-threshold value for pulse width information
      double tot() const { return m_tot; };

     private:
      double m_npe;
      double m_tot;
    };

    using signal_collection = std::vector<signal>;
    signal_collection signals;
  };

} // namespace sand::grain

UFW_DECLARE_UNMANAGED_DATA(sand::reco::digi<sand::grain::photon>)
UFW_DECLARE_MANAGED_DATA(sand::grain::digi)
