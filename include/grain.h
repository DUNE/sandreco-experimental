#pragma once

#include <common.h>
#include <type_traits>

namespace sand {

  namespace grain {

    struct digi : public common::digi_base {

      uint32_t coarse_rising;
      uint16_t coarse_tot;
      uint16_t fine_rising;
      uint32_t charge;

    };

    static_assert(std::is_trivial<digi>::value);

  }

}
