#pragma once

#include "ufw/utils.hpp"
#include <memory>

#include <common/sand.h>

namespace sand::grain {

  constexpr std::size_t camera_height = 32u;
  constexpr std::size_t camera_width  = 32u;

  // We cannot quite use SMatrix as is because its default initialization does not support non-numeric types.
  template <typename T>
  class pixel_array : public ROOT::Math::SMatrix<T, camera_height, camera_width> {
   public:
    pixel_array() : ROOT::Math::SMatrix<T, camera_height, camera_width>(ROOT::Math::SMatrixNoInit()) {}
  };

  enum optics_type : uint8_t {
    mask  = 1,
    mura  = 2,
    lens  = 4,
    doped = 8,
  };

  using index_3d = ROOT::Math::PositionVector3D<ROOT::Math::Cartesian3D<size_t>>;
  using size_3d  = ROOT::Math::DisplacementVector3D<ROOT::Math::Cartesian3D<size_t>>;

  template <typename T>
  class voxel_array {
   public:
    voxel_array(size_3d sz) : m_data(new T[count(sz)]), m_size(sz) {}

    voxel_array(size_3d sz, T init) : m_data(new T[count(sz)]), m_size(sz) { std::fill_n(data(), count(sz), init); }

    voxel_array(size_3d sz, const T* raw) : m_data(new T[count(sz)]), m_size(sz) {
      std::copy_n(raw, count(sz), data());
    }

    voxel_array(const voxel_array&) = delete;

    voxel_array(voxel_array&&) = default;

    voxel_array& operator= (const voxel_array&) = delete;

    voxel_array& operator= (voxel_array&&) = default;

    voxel_array clone() const { return voxel_array(m_size, data()); }

    bool contains(index_3d i) const { return i.x() < m_size.x() && i.y() < m_size.y() && i.z() < m_size.z(); }

    T at(index_3d i) const {
      if (!contains(i)) {
        UFW_EXCEPT(std::out_of_range, fmt::format("voxel_array::at out of bounds {}, {}, {}.", i.x(), i.y(), i.z()));
      }
      return m_data[linear(i)];
    }

    T& at(index_3d i) {
      if (!contains(i)) {
        UFW_EXCEPT(std::out_of_range, fmt::format("voxel_array::at out of bounds {}, {}, {}.", i.x(), i.y(), i.z()));
      }
      return m_data[linear(i)];
    }

    const T* begin() const { return data(); }

    T* begin() { return data(); }

    const T* data() const { return m_data.get(); }

    T* data() { return m_data.get(); }

    const T* end() const { return data() + count(m_size); }

    T* end() { return data() + count(m_size); }

    index_3d index(size_t i) const {
      if (i >= count(m_size)) {
        UFW_EXCEPT(std::out_of_range, fmt::format("voxel_array::index out of bounds {}.", i));
      }
      size_t x = i / (m_size.y() * m_size.z());
      i        = i % (m_size.y() * m_size.z());
      size_t y = i / m_size.z();
      size_t z = i % m_size.z();
      return index_3d(x, y, z);
    }

    size_t linear(index_3d i) const { return (i.x() * m_size.y() + i.y()) * m_size.z() + i.z(); }

    size_3d size() const { return m_size; }

    template <typename Func, typename... Args>
    void for_each(Func&& f, Args&&... args) const {
      for (size_t x = 0u; x != m_size.x(); ++x) {
        for (size_t y = 0u; y != m_size.y(); ++y) {
          for (size_t z = 0u; z != m_size.z(); ++z) {
            index_3d idx(x, y, z);
            std::forward<Func>(f)(idx, m_data[linear(idx)], std::forward<Args...>(args)...);
          }
        }
      }
    }

    template <typename Func, typename... Args>
    void for_each(Func&& f, Args&&... args) {
      for (size_t x = 0u; x != m_size.x(); ++x) {
        for (size_t y = 0u; y != m_size.y(); ++y) {
          for (size_t z = 0u; z != m_size.z(); ++z) {
            index_3d idx(x, y, z);
            std::forward<Func>(f)(idx, m_data[linear(idx)], std::forward<Args...>(args)...);
          }
        }
      }
    }

   private:
    static size_t count(size_3d sz) { return sz.x() * sz.y() * sz.z(); }

   private:
    std::unique_ptr<T[]> m_data;
    size_3d m_size;
  };

} // namespace sand::grain
