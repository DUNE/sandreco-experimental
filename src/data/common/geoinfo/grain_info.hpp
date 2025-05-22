#pragma once

#include <geoinfo/subdetector_info.hpp>

namespace sand {

  class geoinfo::grain_info : public subdetector_info {

  public:
    grain_info(const geoinfo&, const std::string&);

    virtual ~grain_info();

    using subdetector_info::path;

    geo_id id(const geo_path&) const override;

    geo_path path(geo_id) const override;

  };

}
