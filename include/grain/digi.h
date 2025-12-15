#pragma once

#include <vector>

#include <ufw/data.hpp>
#include <common/sand.h>
#include <common/truth.h>
#include <grain/grain.h>

namespace sand::grain {

  struct digi : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    struct signal : public sand::truth {
      /// The readout channel associated with the signal.
      channel_id channel;
      /// Calibrated time [ns] of rising edge since start of context.
      double time_rising_edge;
      /// Calibrated time [ns] over threshold.
      double time_over_threshold;
      /// Calibrated signal amplitude [photoelectrons].
      double npe;
    };

    using signal_collection = std::vector<signal>;
    signal_collection signals;
  };

} // namespace sand::grain

UFW_DECLARE_MANAGED_DATA(sand::grain::digi)
