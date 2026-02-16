#pragma once

#include <ufw/data.hpp>
#include <common/digi.h>
#include <common/sand.h>

namespace sand::ecal {
  struct digi : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    struct signal : reco::digi {
        double adc;
        double tdc;
        double tot;
    };

    using signal_collection = std::vector<signal>;
    signal_collection signals;
  };
} // namespace sand::ecal

UFW_DECLARE_MANAGED_DATA(sand::ecal::digi);
