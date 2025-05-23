#pragma once

#include <common/sand.h>

namespace sand::grain {

  constexpr std::size_t camera_height = 32u;
  constexpr std::size_t camera_width = 32u;

  //We cannot quite use SMatrix as is because its default initialization does not support non-numeric types.
  template <typename T>
  class pixel_array : public ROOT::Math::SMatrix<T, camera_height, camera_width> {
  public:
    pixel_array() : ROOT::Math::SMatrix<T, camera_height, camera_width>(ROOT::Math::SMatrixNoInit()) {}
  };

  enum optics_type : uint8_t {
    mask = 1,
    mura = 2,
    lens = 4,
    doped = 8,
  };

}
