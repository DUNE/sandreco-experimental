#pragma once

#include <common/sand.h>

namespace sand {

  template <typename T>
  struct scalar : managed_data_base {
    using value_type = T;
    value_type value;
  };

} // namespace sand

template struct sand::scalar<bool>;
UFW_DECLARE_MANAGED_DATA(sand::scalar<bool>)

template struct sand::scalar<int8_t>;
UFW_DECLARE_MANAGED_DATA(sand::scalar<int8_t>)

template struct sand::scalar<uint8_t>;
UFW_DECLARE_MANAGED_DATA(sand::scalar<uint8_t>)

template struct sand::scalar<int16_t>;
UFW_DECLARE_MANAGED_DATA(sand::scalar<int16_t>)

template struct sand::scalar<uint16_t>;
UFW_DECLARE_MANAGED_DATA(sand::scalar<uint16_t>)

template struct sand::scalar<int32_t>;
UFW_DECLARE_MANAGED_DATA(sand::scalar<int32_t>)

template struct sand::scalar<uint32_t>;
UFW_DECLARE_MANAGED_DATA(sand::scalar<uint32_t>)

template struct sand::scalar<int64_t>;
UFW_DECLARE_MANAGED_DATA(sand::scalar<int64_t>)

template struct sand::scalar<uint64_t>;
UFW_DECLARE_MANAGED_DATA(sand::scalar<uint64_t>)

template struct sand::scalar<float>;
UFW_DECLARE_MANAGED_DATA(sand::scalar<float>)

template struct sand::scalar<double>;
UFW_DECLARE_MANAGED_DATA(sand::scalar<double>)

template struct sand::scalar<sand::pos_3d>;
UFW_DECLARE_MANAGED_DATA(sand::scalar<sand::pos_3d>)

template struct sand::scalar<sand::dir_3d>;
UFW_DECLARE_MANAGED_DATA(sand::scalar<sand::dir_3d>)

template struct sand::scalar<sand::vec_4d>;
UFW_DECLARE_MANAGED_DATA(sand::scalar<sand::vec_4d>)
