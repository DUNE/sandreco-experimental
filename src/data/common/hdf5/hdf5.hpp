
#pragma once

#include <common/data.h>
#include <ufw/config.hpp>

#include <H5Cpp.h>
#include <H5DataType.h>
#include <H5PredType.h>

namespace sand::hdf5 {

  class ndarray : public ufw::data::base<ufw::data::complex_tag, ufw::data::instanced_tag, ufw::data::global_tag> {
   public:
    class ndrange : public std::vector<hsize_t> {
     public:
      using std::vector<hsize_t>::vector;
      size_t byte_size() const { return flat_size() * type().getSize(); }
      size_t flat_size() const;
      void set_type(H5::DataType t) { m_type = t; }
      H5::DataType type() const { return m_type; }

     private:
      H5::DataType m_type;
    };

   public:
    ndarray(const ufw::config&);

    virtual ~ndarray() = default;

    std::string attribute(const std::string& dataset, const std::string& attr);

    /**
     * Note that attributes can only be set on existing datasets, e.g. as created by write()
     */
    void set_attribute(const std::string& dataset, const std::string& attr, const std::string& value);

    /**
     * List all the datasets in this file.
     */
    const std::vector<std::string>& datasets() const { return m_datasets; }

    /**
     * Provides metadata on the given dataset.
     */
    ndrange range(const std::string&) const;

    /**
     * Reads the entire dataset into the user provided pointer.
     * Memory must allocated by the user in the required size.
     */
    void read(const std::string&, void*);
    /**
     * Writes the entire dataset from the user provided pointer.
     * Does not take ownership of the memory.
     */
    void write(const std::string&, const ndrange&, const void*);

    /**
     * Reads the entire dataset into the user provided object.
     * The object must provide sufficient space, either by pointing to adequate memory,
     * by being a container with contiguous memory, large enough and exposing a @p data() member,
     * or simply by being large enough itself.
     */
    template <typename T>
    void read(const std::string& dataset, T& usrobj) {
      if constexpr (std::is_pointer_v<T>) {
        read(dataset, static_cast<void*>(usrobj));
      } else if constexpr (std::is_convertible_v<decltype(std::declval<T>().data()), void*>) {
        read(dataset, static_cast<void*>(usrobj.data()));
      } else {
        read(dataset, static_cast<void*>(&usrobj));
      }
    }

    template <typename T>
    std::unique_ptr<T[]> read(const std::string& dataset) {
      auto ndr = range(dataset);
      UFW_ASSERT(ndr.type().getSize() == sizeof(T), "Reading to type of incorrect size.");
      T* raw = new T[ndr.flat_size()];
      read(dataset, static_cast<void*>(raw));
      return std::unique_ptr<T[]>(raw);
    }

    template <typename T>
    void write(const std::string& dataset, const ndrange& ndr, const T& usrobj) {
      if constexpr (std::is_pointer_v<T>) {
        write(dataset, ndr, static_cast<const void*>(usrobj));
      } else if constexpr (std::is_convertible_v<decltype(std::declval<T>().data()), const void*>) {
        write(dataset, ndr, static_cast<const void*>(usrobj.data()));
      } else {
        write(dataset, ndr, static_cast<const void*>(&usrobj));
      }
    }

   private:
    ufw::config::io m_io_type;
    H5::H5File m_file;
    std::vector<std::string> m_datasets;
  };

} // namespace sand::hdf5

template <>
struct fmt::formatter<sand::hdf5::ndarray::ndrange> : formatter<string_view> {
  auto format(const sand::hdf5::ndarray::ndrange& r, format_context& ctx) const -> format_context::iterator {
    auto szof = r.type().getSize();
    std::string dims;
    for (auto d: r) {
      dims += std::to_string(d);
      dims += ", ";
    }
    if (!dims.empty()) {
      dims.resize(dims.size() - 2);
    }
    return fmt::format_to(ctx.out(), "H5<{}>({})", szof, dims);
  }
};


UFW_DECLARE_COMPLEX_DATA(sand::hdf5::ndarray);
