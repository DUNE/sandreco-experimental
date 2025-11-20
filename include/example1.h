#pragma once

#include <ufw/data.hpp>

namespace sand {

  struct example1 : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    std::size_t uid;
    std::vector<float> times;
  };

} // namespace sand

UFW_DECLARE_MANAGED_DATA(sand::example1)
