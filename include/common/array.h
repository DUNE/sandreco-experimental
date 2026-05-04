#pragma once

#include <common/sand.h>

namespace sand {

  template <typename T>
  struct array : managed_data_base {
    using value_type = T;
    using array_type = std::vector<value_type>;
    array_type values;
  };

} // namespace sand

#define SANDRECO_DEFINE_ARRAY_FOR_TYPE(T) \
template struct sand::array<T>; \
UFW_DECLARE_MANAGED_DATA(sand::array<T>)

SANDRECO_DEFINE_ARRAY_FOR_TYPE(bool)
SANDRECO_DEFINE_ARRAY_FOR_TYPE(int8_t)
SANDRECO_DEFINE_ARRAY_FOR_TYPE(uint8_t)
SANDRECO_DEFINE_ARRAY_FOR_TYPE(int16_t)
SANDRECO_DEFINE_ARRAY_FOR_TYPE(uint16_t)
SANDRECO_DEFINE_ARRAY_FOR_TYPE(int32_t)
SANDRECO_DEFINE_ARRAY_FOR_TYPE(uint32_t)
SANDRECO_DEFINE_ARRAY_FOR_TYPE(int64_t)
SANDRECO_DEFINE_ARRAY_FOR_TYPE(uint64_t)
SANDRECO_DEFINE_ARRAY_FOR_TYPE(float)
SANDRECO_DEFINE_ARRAY_FOR_TYPE(double)
SANDRECO_DEFINE_ARRAY_FOR_TYPE(sand::pos_3d)
SANDRECO_DEFINE_ARRAY_FOR_TYPE(sand::dir_3d)
SANDRECO_DEFINE_ARRAY_FOR_TYPE(sand::vec_4d)
