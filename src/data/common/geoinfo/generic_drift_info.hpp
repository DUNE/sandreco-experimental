#pragma once

#include <geoinfo/tracker_info.hpp>

namespace sand {

  class geoinfo::generic_drift_info : public tracker_info {

  public:
    struct station : public tracker_info::station {
      std::vector<geo_id> geos; ///< The unique geometry identifier
      wire_list view(int number) const;
      void generate_drift_view(const geo_path &, const geo_id &);
      void generate_wire_list(const size_t &);
    };

    generic_drift_info(const geoinfo&, const geo_path&, const ufw::config&);

    virtual ~generic_drift_info();

    using subdetector_info::path;

    geo_id id(const geo_path&) const override;

    geo_path path(geo_id) const override;

    const std::vector<double> & view_angle() const { return m_view_angle; }
    const std::vector<double> & view_offset() const { return m_view_offset; }
    const std::vector<double> & view_spacing() const { return m_view_spacing; }
    double distance_between_views() const { return m_distance_between_views; }

  private:

    std::vector<double> m_view_angle ;
    std::vector<double> m_view_offset ;
    std::vector<double> m_view_spacing ;
    double m_distance_between_views ;
    
  };

} // namespace sand
