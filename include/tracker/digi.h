
#pragma once

#include <ufw/data.hpp>
#include <common/digi.h>
#include <common/sand.h>
#include <common/truth.h>

namespace sand::tracker {

  struct digi : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    /**
     * @brief A signal recorded by a tracker channel.
     */
    struct signal : reco::digi {

      /**
       * @brief Time-to-digital converter (TDC) value.
       * @unit
       */
      double tdc;

      /**
       * @brief Analog-to-digital converter (ADC) value.
       * @unit ADC counts (arbitrary units)
       */
      double adc;
    };

    using signal_collection = std::vector<signal>;
    signal_collection signals;
  };
} // namespace sand::tracker

UFW_DECLARE_MANAGED_DATA(sand::tracker::digi)
