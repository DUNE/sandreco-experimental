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


template struct sand::array<bool>;
UFW_DECLARE_MANAGED_DATA(sand::array<bool>)

template struct sand::array<int8_t>;
UFW_DECLARE_MANAGED_DATA(sand::array<int8_t>)

template struct sand::array<uint8_t>;
UFW_DECLARE_MANAGED_DATA(sand::array<uint8_t>)

template struct sand::array<int16_t>;
UFW_DECLARE_MANAGED_DATA(sand::array<int16_t>)

template struct sand::array<uint16_t>;
UFW_DECLARE_MANAGED_DATA(sand::array<uint16_t>)

template struct sand::array<int32_t>;
UFW_DECLARE_MANAGED_DATA(sand::array<int32_t>)

template struct sand::array<uint32_t>;
UFW_DECLARE_MANAGED_DATA(sand::array<uint32_t>)

template struct sand::array<int64_t>;
UFW_DECLARE_MANAGED_DATA(sand::array<int64_t>)

template struct sand::array<uint64_t>;
UFW_DECLARE_MANAGED_DATA(sand::array<uint64_t>)

template struct sand::array<float>;
UFW_DECLARE_MANAGED_DATA(sand::array<float>)

template struct sand::array<double>;
UFW_DECLARE_MANAGED_DATA(sand::array<double>)

template struct sand::array<sand::pos_3d>;
UFW_DECLARE_MANAGED_DATA(sand::array<sand::pos_3d>)

template struct sand::array<sand::dir_3d>;
UFW_DECLARE_MANAGED_DATA(sand::array<sand::dir_3d>)

template struct sand::array<sand::vec_4d>;
UFW_DECLARE_MANAGED_DATA(sand::array<sand::vec_4d>)
