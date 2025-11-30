
#pragma once

#include <ufw/config.hpp>
#include <ufw/data.hpp>

#include <H5Cpp.h>

namespace sand::hdf5 {

  class ndarray : public ufw::data::base<ufw::data::complex_tag, ufw::data::instanced_tag, ufw::data::global_tag> {
   public:
    class ndrange : public std::vector<hsize_t> {
     public:
      using std::vector<hsize_t>::vector;
      size_t flat_size() const;
      void set_type(H5T_class_t t) { m_type = t; }
      H5T_class_t type() const { return m_type; }

     private:
      H5T_class_t m_type;
    };

   public:
    ndarray(const ufw::config&);

    virtual ~ndarray() = default;

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
     * Reads the entire dataset into the user provided object.
     * Memory must allocated by the user in the required size.
     */
    template <typename T>
    void read(const std::string& dataset, T& usrobj) {
      if constexpr (std::is_pointer_v<T>) {
        using ValT = std::remove_pointer_t<T>;
        read(dataset, static_cast<void*>(usrobj));
      } else if constexpr (std::is_convertible_v<decltype(std::declval<T>().data()), void*>) {
        read(dataset, static_cast<void*>(usrobj.data()));
      } else {
        read(dataset, static_cast<void*>(&usrobj));
      }
      read(dataset, usrobj.data());
    }
    // void write(const std::string&, const void*);

   private:
    H5::H5File m_file;
    std::vector<std::string> m_datasets;
  };

} // namespace sand::hdf5

UFW_DECLARE_COMPLEX_DATA(sand::hdf5::ndarray);
