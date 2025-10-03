#pragma once

#include <memory>

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

  using index_3d = ROOT::Math::PositionVector3D<ROOT::Math::Cartesian3D<size_t>>;
  using size_3d = ROOT::Math::DisplacementVector3D<ROOT::Math::Cartesian3D<size_t>>;

  template <typename T>
  class voxel_array {

    voxel_array(size_3d sz) :
     m_data(new T[count(sz)]), m_size(sz) {}

    voxel_array(size_3d sz, T init) :
     m_data(new T[count(sz)]), m_size(sz) {
      std::fill_n(data(), count(sz), init);
    }

    voxel_array(size_3d sz, const T* raw) :
     m_data(new T[count(sz)]), m_size(sz) {
      std::copy_n(raw, count(sz), data());
    }

    voxel_array(const voxel_array&) = delete;

    voxel_array(voxel_array&&) = default;

    voxel_array& operator = (const voxel_array&) = delete;

    voxel_array& operator = (voxel_array&&) = default;

    voxel_array clone() const {
      return voxel_array(m_size, data());
    }

    T at(index_3d i) const {
      return m_data[linear(i)];
    }

    T& at(index_3d i) {
      return m_data[linear(i)];
    }

    const T* data() const {
      return m_data.get();
    }

    T* data() {
      return m_data.get();
    }

    index_3d index(size_t);

    size_t linear(index_3d);

    size_3d size() const {
      return m_size;
    }

  private:
    static size_t count(size_3d sz ) {
      return sz[0] * sz[1] * sz[2];
    }

  private:
    std::unique_ptr<T[]> m_data;
    size_3d m_size;

  };

}
