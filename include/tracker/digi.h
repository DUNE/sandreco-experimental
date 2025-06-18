#pragma once

#include <vector>

#include <ufw/data.hpp>
#include <common/truth.h>
#include <common/sand.h>

namespace sand::tracker {

  /**
   * @brief Represents a digitized collection of signals in the tracker subsystem.
   */
  struct digi : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {

    /**
     * @brief A signal recorded by a tracker channel.
     */
    struct signal : public true_hits {
      /**
       * @brief The readout channel associated with the signal.
       */
      channel_id channel;

      /**
       * @brief Time of the signal rising edge, measured since the nominal start of the spill.
       * @unit calibrated nanoseconds
       */
      double time_rising_edge;

      /**
       * @brief Charge integral
       * @unit calibrated electrons
       */
      double charge;
    };

    using signal_list = std::vector<signal>;

    signal_list signals;

  };
}

UFW_DECLARE_MANAGED_DATA(sand::tracker::digi)
