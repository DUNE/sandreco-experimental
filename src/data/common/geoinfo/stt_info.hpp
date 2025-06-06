#pragma once

#include <geoinfo/tracker_info.hpp>

namespace sand {

  class geoinfo::stt_info : public tracker_info {

  public:
    struct wire : public tracker_info::wire {
      geo_id geo; ///< The unique geometry identifier
      double straw_radius;
    };

    struct station : public tracker_info::station {
      wire_list x_view() const;
      wire_list y_view() const;
    };

  public:
    stt_info(const geoinfo&);

  };

}
