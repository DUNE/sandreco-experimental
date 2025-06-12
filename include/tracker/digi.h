#pragma once

#include <vector>
#include <cstdint>

#include <ufw/data.hpp>
#include <common/truth.h>
#include <common/sand.h>

#pragma once

#include <vector>
#include <cstdint>

#include <ufw/data.hpp>
#include <common/truth.h>
#include <common/sand.h>

namespace sand::tracker {

  /**
   * @brief Represents a digitized collection of signals in the tracker subsystem.
   *
   * Inherits from:
   * - sand::true_hits: Base class representing true simulation hits.
   * - ufw::data::base: Provides data handling with managed, instanced, and context tags.
   */
  struct digi : public sand::true_hits,
                ufw::data::base<ufw::data::managed_tag,
                                ufw::data::instanced_tag,
                                ufw::data::context_tag> {

    /**
     * @brief A signal recorded by a tracker channel.
     */
    struct signal {
      /**
       * @brief The readout channel associated with the signal.
       */
      channel_id channel;

      /**
       * @brief Time-to-digital converter (TDC) value.
       * @unit nanoseconds (ns)
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
}

UFW_DECLARE_MANAGED_DATA(sand::tracker::digi)
