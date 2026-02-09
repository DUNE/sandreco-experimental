#pragma once

#include <ufw/data.hpp>
#include <common/sand.h>

namespace sand::ecal {
  struct pes : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    struct pe {
      double arrival_time;
    };
    using pe_collection = std::map<channel_id, std::vector<pe>>;
    pe_collection collection;
  };
} // namespace sand::ecal

UFW_DECLARE_MANAGED_DATA(sand::ecal::pes);
