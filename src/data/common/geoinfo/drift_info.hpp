#pragma once

#include <geoinfo/tracker_info.hpp>

namespace sand {

  class geoinfo::drift_info : public tracker_info {

    struct station : public tracker_info::station {
      geo_id geo; ///< The unique geometry identifier
      wire_list x_view() const;
      wire_list u_view() const;
      wire_list v_view() const;
    };

  public:
    drift_info(const geoinfo&);

    geo_id id(const geo_path&) const override;

    geo_path path(geo_id) const override;

  };

}
