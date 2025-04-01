#pragma once

#include <ufw/data.hpp>

namespace sand {

  struct example2 : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    double base;
    std::vector<double> weights;
  };

}

UFW_DECLARE_MANAGED_DATA(sand::example2)
