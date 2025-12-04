#pragma once

#include <geoinfo/tracker_info.hpp>

namespace sand {

  class geoinfo::drift_info : public tracker_info {
    struct station : public tracker_info::station {
      geo_id geo_x, geo_u, geo_v; ///< The unique geometry identifier
      wire_list x_view() const;
      wire_list u_view() const;
      wire_list v_view() const;
      void set_drift_view(const geo_path &, const geo_id &);
      void set_wire_list(const size_t &);
    };

   public:
    drift_info(const geoinfo&);

    virtual ~drift_info();

    using subdetector_info::path;

    geo_id id(const geo_path&) const override;

    geo_path path(geo_id) const override;

    static constexpr std::array<double, 3> view_angle = {
        0.0,
        -M_PI / 36.0,
        M_PI / 36.0
    };

    static constexpr std::array<double, 3> view_offset = {
        10.0, 10.0, 10.0
    };

    static constexpr std::array<double, 3> view_spacing = {
        10.0, 10.0, 10.0
    };

    static constexpr std::array<double, 3> view_length = {
        0.0, 0.0, 0.0
    };

    static constexpr std::array<double, 3> view_velocity = {
        0.05, 0.05, 0.05
    };
    
  };

} // namespace sand
