#pragma once

#include <vector>
#include <cstdint>

#include <ufw/data.hpp>
#include <common/truth.h>
#include <common/sand.h>

namespace sand::tracker {

  struct digi : public sand::true_hits, ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    geo_id wire;
    channel_id channel;
    double tdc; 
    double adc;
  };
}

UFW_DECLARE_MANAGED_DATA(sand::tracker::digi)
