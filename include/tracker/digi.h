/**
 * @file digi.h
 * @author fbattist (federico.battisti@bo.infn.it)
 * @brief The standard tracker digitization data structure.
 * @version 0.1
 * @date 2025-06-12
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <ufw/data.hpp>
#include <common/sand.h>
#include <common/truth.h>

namespace sand::tracker {

  /**
   * @brief Represents a digitized collection of signals in the tracker subsystem.
   *
   * Inherits from:
   * - sand::truth: Base class representing true simulation hits.
   * - ufw::data::base: Provides data handling with managed, instanced, and context tags.
   */
  struct digi
    : public sand::truth
    , ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
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
