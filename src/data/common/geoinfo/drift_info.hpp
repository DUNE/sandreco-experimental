#pragma once

#include <geoinfo/tracker_info.hpp>

namespace sand {

  class geoinfo::drift_info : public tracker_info {

    struct plane : public tracker_info::plane {
      geo_id geo; ///< The unique geometry identifier
    };

  public:
    drift_info(const geoinfo&);

  };

}
