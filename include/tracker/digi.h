
#pragma once

#include <common/digi.h>
#include <common/sand.h>
#include <common/truth.h>

namespace sand::tracker {

  struct digi : managed_data_base {
    /**
     * @brief A signal recorded by a tracker channel.
     */
    struct signal : reco::digi<sand::truth_index> {

      signal() {}

      signal(channel_id ch, time t, double q) :
        reco::digi<sand::truth_index>(ch, t, source::sim), m_adc(q) {}
      /**
       * @brief Time-to-digital converter (TDC) value.
       * @unit ns
       */
      double tdc() const { return t().best(); }

      /**
       * @brief Analog-to-digital converter (ADC) value.
       * @unit ADC counts (arbitrary units)
       */
      double adc() const { return m_adc; }

    private:
      double m_adc = NAN;
    };

    using signal_collection = std::vector<signal>;
    signal_collection signals;
  };
} // namespace sand::tracker

UFW_DECLARE_MANAGED_DATA(sand::tracker::digi)
