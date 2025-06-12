#pragma once

#include <vector>
#include <cstdint>

#include <ufw/data.hpp>
#include <common/truth.h>
#include <common/sand.h>

namespace sand::tracker {

  struct digi : public sand::true_hits, ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    struct signal {
      channel_id channel;  // The channel associated with the signal
      double tdc;  // Time-to-digital converter value
      double adc;  // Analog-to-digital converter value
    };
    
    using signal_collection = std::vector<signal>;
    signal_collection signals;  // Collection of signals associated with the digi
  };
}

UFW_DECLARE_MANAGED_DATA(sand::tracker::digi)
