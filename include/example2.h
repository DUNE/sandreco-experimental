#pragma once

#include <data.hpp>

namespace sand {

  struct example2 : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    std::size_t uid;
    double base;
    std::vector<float> weights;
  };

}

UFW_DECLARE_MANAGED_DATA(sand::example2)
