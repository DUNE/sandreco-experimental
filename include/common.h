#pragma once

#include <cstdint>
#include <type_traits>

namespace sand {

  namespace common {

    struct digi_base {

      uint16_t detector_id;
      uint16_t module_id;
      uint32_t channel_id;
      uint64_t timestamp;

    };

    static_assert(std::is_trivial<digi_base>::value);

  }

}
