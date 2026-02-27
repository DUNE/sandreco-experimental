#pragma once

#include <ufw/data.hpp>
#include <ecal/digi.h>

namespace sand::ecal {
  struct slices : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    using slice            = digi::signal_collection;
    using slice_collection = std::vector<slice>;
    slice_collection collection;
  };
} // namespace sand::ecal

UFW_DECLARE_MANAGED_DATA(sand::ecal::slices);
