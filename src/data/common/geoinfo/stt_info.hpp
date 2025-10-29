#pragma once

#include <geoinfo/tracker_info.hpp>
#include <optional>

namespace sand {

  class geoinfo::stt_info : public tracker_info {

  public:
    struct wire : public tracker_info::wire {
      geo_id geo; ///< The unique geometry identifier
      double straw_radius;
      std::optional<std::pair<vec_4d, vec_4d>> closest_points(const vec_4d&, const vec_4d&, const double&) const;
      double get_min_time(const vec_4d&, const double &) const;
    };

    struct station : public tracker_info::station {
      wire_list x_view() const;
      wire_list y_view() const;
    };

  public:
    stt_info(const geoinfo&);

    virtual ~stt_info();

    using subdetector_info::path;

    geo_id id(const geo_path&) const override;

    geo_path path(geo_id) const override;

    const wire* get_wire_by_id(const geo_id& id) const;

  };

}
