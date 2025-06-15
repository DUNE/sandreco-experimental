#pragma once

#include <geoinfo/subdetector_info.hpp>

namespace sand {

  class geoinfo::grain_info : public subdetector_info {

  public:
    grain_info(const geoinfo&);

    virtual ~grain_info();

    geo_id id(const geo_path&) const override;

    geo_path path(geo_id) const override;

  };

}
