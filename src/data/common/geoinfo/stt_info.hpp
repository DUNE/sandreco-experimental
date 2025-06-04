#pragma once

#include <geoinfo/tracker_info.hpp>

namespace sand {

  class geoinfo::stt_info : public tracker_info {

  public:
    struct wire : public tracker_info::wire {
      geo_id geo; ///< The unique geometry identifier
      double straw_radius;
    };

  public:
    stt_info(const geoinfo&);

  };

}
