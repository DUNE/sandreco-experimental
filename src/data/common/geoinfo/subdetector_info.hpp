#pragma once

#include <geoinfo/geoinfo.hpp>

namespace sand {

  class geoinfo::subdetector_info {

  public:
    subdetector_info(const geoinfo&, const geo_path&);
    subdetector_info(const subdetector_info&) = delete;
    subdetector_info& operator = (const subdetector_info&) = delete;

    const pos_3d& centre() const { return m_centre; }

  protected:
    const geoinfo& info() { return r_info; }

  private:
    const geoinfo& r_info;
    pos_3d m_centre;

  };

}
