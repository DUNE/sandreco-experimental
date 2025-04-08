#pragma once

#include <ufw/data.hpp>

namespace sand::grain {

  struct digi : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {

  };

}

UFW_DECLARE_MANAGED_DATA(sand::grain::digi)
