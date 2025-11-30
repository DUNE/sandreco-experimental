
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

    const std::vector<std::string>& datasets() const { return m_datasets; }

    ndrange range(const std::string&) const;

    void read(const std::string&, void*);

    // void write(const std::string&, const void*);

   private:
    H5::H5File m_file;
    std::vector<std::string> m_datasets;
  };

} // namespace sand::hdf5

UFW_DECLARE_COMPLEX_DATA(sand::hdf5::ndarray);
