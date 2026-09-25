#pragma once

#include <array>

#include <common/sand.h>

namespace sand::grain {

  constexpr std::size_t camera_height = 32u;
  constexpr std::size_t camera_width  = 32u;

  // We cannot quite use SMatrix as is because its default initialization does not support non-numeric types.
  template <typename T, std::size_t Height = camera_height, std::size_t Width = camera_width>
  class pixel_array {
   public:
    // Default constructor sets the size to the template defaults
    pixel_array() {}

    // Standard copy/move behavior
    pixel_array(const pixel_array&)             = default;
    pixel_array(pixel_array&&)                  = default;
    pixel_array& operator= (const pixel_array&) = default;
    pixel_array& operator= (pixel_array&&)      = default;

    // Helper to get the flat index
    size_t linear(std::size_t x, std::size_t y) const { return (x * Width) + y; }

    // Accessors
    T& at(std::size_t x, std::size_t y) { return m_data[linear(x, y)]; }
    const T& at(std::size_t x, std::size_t y) const { return m_data[linear(x, y)]; }

    // Data access for bulk operations
    T* data() { return m_data.data(); }
    const T* data() const { return m_data.data(); }

    size_t size() const { return m_data.size(); }
    static constexpr std::size_t width  = Width;
    static constexpr std::size_t height = Height;

    // For loops / iteration logic
    template <typename Func, typename... Args>
    void for_each(Func&& f, Args&&... args) const {
      for (std::size_t x = 0; x < Height; ++x) {
        for (std::size_t y = 0; y < Width; ++y) {
          f(x, y, m_data[linear(x, y)], std::forward<Args>(args)...);
        }
      }
    }

    typename std::array<T, Width * Height>::iterator begin() { return m_data.begin(); }
    typename std::array<T, Width * Height>::iterator end() { return m_data.end(); }

    typename std::array<T, Width * Height>::const_iterator begin() const { return m_data.begin(); }
    typename std::array<T, Width * Height>::const_iterator end() const { return m_data.end(); }

   private:
    std::array<T, Width * Height> m_data;
  };

  enum optics_type : uint8_t {
    mask  = 1,
    mura  = 2,
    lens  = 4,
    doped = 8,
  };

  using index_3d = ROOT::Math::PositionVector3D<ROOT::Math::Cartesian3D<size_t>>;
  using size_3d  = ROOT::Math::DisplacementVector3D<ROOT::Math::Cartesian3D<size_t>>;
  // Has metric (-,-,-,+) but sould not be used in scalar products anyway
  using size_4d = ROOT::Math::LorentzVector<ROOT::Math::PxPyPzE4D<size_t>>;

  template <typename T>
  class voxel_array {
   public:
    voxel_array(size_3d sz) : m_data(count(sz)), m_size(sz) {}

    voxel_array(size_3d sz, T init) : m_data(count(sz), init), m_size(sz) {}

    voxel_array(size_3d sz, const T* raw) : m_data(count(sz)), m_size(sz) { std::copy_n(raw, count(sz), data()); }

    voxel_array() : m_data(), m_size(0, 0, 0) {}

    voxel_array(const voxel_array&) = default;

    voxel_array(voxel_array&&) = default;

    voxel_array& operator= (const voxel_array&) = default;

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

    const T* data() const { return m_data.data(); }

    T* data() { return m_data.data(); }

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

    xform_3d xform_id_to_fiducial(dir_3d voxel_size) const {
      return xform_3d(voxel_size.x(), 0.0, 0.0, (0.5 - 0.5 * m_size.x()) * voxel_size.x(), 0.0, voxel_size.y(), 0.0,
                      (0.5 - 0.5 * m_size.y()) * voxel_size.y(), 0.0, 0.0, voxel_size.z(),
                      (0.5 - 0.5 * m_size.z()) * voxel_size.z());
    }

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
    std::vector<T> m_data;
    size_3d m_size;
  };

} // namespace sand::grain

template <>
struct fmt::formatter<sand::grain::index_3d> : formatter<string_view> {
  auto format(const sand::grain::index_3d& c, format_context& ctx) const -> format_context::iterator {
    return fmt::format_to(ctx.out(), "({}, {}, {})", c.x(), c.y(), c.z());
  }
};

template <>
struct fmt::formatter<sand::grain::size_3d> : formatter<string_view> {
  auto format(const sand::grain::size_3d& c, format_context& ctx) const -> format_context::iterator {
    return fmt::format_to(ctx.out(), "({}, {}, {})", c.x(), c.y(), c.z());
  }
};

template <>
struct fmt::formatter<sand::grain::size_4d> : formatter<string_view> {
  auto format(const sand::grain::size_4d& c, format_context& ctx) const -> format_context::iterator {
    return fmt::format_to(ctx.out(), "({}, {}, {}, {})", c.x(), c.y(), c.z(), c.t());
  }
};

UFW_DECLARE_UNMANAGED_DATA(ROOT::Math::Cartesian3D<size_t>)
UFW_DECLARE_UNMANAGED_DATA(ROOT::Math::PxPyPzE4D<size_t>)
UFW_DECLARE_UNMANAGED_DATA(ROOT::Math::DisplacementVector3D<ROOT::Math::Cartesian3D<size_t>>)
UFW_DECLARE_UNMANAGED_DATA(ROOT::Math::PositionVector3D<ROOT::Math::Cartesian3D<size_t>>)
UFW_DECLARE_UNMANAGED_DATA(ROOT::Math::LorentzVector<ROOT::Math::PxPyPzE4D<size_t>>)
