#pragma once

#include <common/digi.h>
#include <common/sand.h>

#include <vector>

namespace sand::tracker {

  struct digi_container : managed_data_base {
    struct digi {
      /**
       * @brief A signal contributing to a digi: the raw arrival time (t()) and the deposited
       * energy of one hit on a tracker channel.
       */
      struct signal : reco::digi<sand::truth_index> {
        signal() {}

        signal(channel_id ch, time t, double e) : reco::digi<sand::truth_index>(ch, t, source::sim), m_energy(e) {}

        /**
         * @brief Deposited energy carried by the signal.
         */
        double energy() const { return m_energy; }

       private:
        double m_energy = NAN;
      };

      using signal_collection = std::vector<signal>;
      signal_collection signals;

      /**
       * @brief Time-to-digital converter (TDC) value of the digi.
       * @unit ns
       */
      double tdc() const { return m_tdc; }

      /**
       * @brief Analog-to-digital converter (ADC) value of the digi.
       * @unit ADC counts
       */
      double adc() const { return m_adc; }

      void set_tdc(double tdc) { m_tdc = tdc; }
      void set_adc(double adc) { m_adc = adc; }

     private:
      double m_tdc = NAN;
      double m_adc = NAN;
    };

    /// @brief Vector container for digits
    using digi_collection = std::vector<digi>;

    /// @brief Collection of digits of the event
    digi_collection digis;
  };
} // namespace sand::tracker

UFW_DECLARE_MANAGED_DATA(sand::tracker::digi_container)
