#pragma once

#include <geoinfo/geoinfo.hpp>

namespace sand {

  class geoinfo::subdetector_info {

  public:
    subdetector_info(const geoinfo&, const geo_path&);
    subdetector_info(const subdetector_info&) = delete;
    subdetector_info& operator = (const subdetector_info&) = delete;

    virtual ~subdetector_info();

    virtual geo_id id(const geo_path&) const = 0;

    virtual geo_path path(geo_id) const = 0;

    geo_path path() const { return m_path; }

    const xform_3d& transform() const { return m_transform; }

  protected:
    const geoinfo& info() { return r_info; }

  private:
    const geoinfo& r_info;
    geo_path m_path;
    xform_3d m_transform;

  };

}
