#pragma once

#include <common/version.h>
#include <ufw/data.hpp>

namespace sand {

  using managed_data_base = ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag>;

}
