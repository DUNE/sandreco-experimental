#pragma once

#include <geoinfo/tracker_info.hpp>

namespace sand {

  class geoinfo::drift_info : public tracker_info {

  public:
    struct station : public tracker_info::station {
      geo_id geo_x, geo_u, geo_v; ///< The unique geometry identifier
      wire_list x_view() const;
      wire_list u_view() const;
      wire_list v_view() const;
      void set_drift_view(const geo_path &, const geo_id &, std::vector<std::unique_ptr<wire>> &);
      void set_wire_list(const size_t &, std::vector<std::unique_ptr<wire>> &);
    };

    drift_info(const geoinfo&, const std::array<double, 3>&, const std::array<double, 3>&, const std::array<double, 3>&);

    virtual ~drift_info();

    using subdetector_info::path;

    geo_id id(const geo_path&) const override;

    geo_path path(geo_id) const override;

    const std::array<double, 3> & view_angle() const { return m_view_angle; }
    const std::array<double, 3> & view_offset() const { return m_view_offset; }
    const std::array<double, 3> & view_spacing() const { return m_view_spacing; }
    void set_wire_adjecency(std::vector<std::unique_ptr<wire>> & ws);

  private:

    std::array<double, 3> m_view_angle ;
    std::array<double, 3> m_view_offset ;
    std::array<double, 3> m_view_spacing ;
    
  };

} // namespace sand
