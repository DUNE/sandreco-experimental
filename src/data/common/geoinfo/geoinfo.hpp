#pragma once

#include <common/sand.h>

#include <ufw/config.hpp>

namespace sand {

  /**
   * Implementation guide: all members that belong to a specific subdetector go in its _info class.
   * Keep only general purpose members in the main class, and keep the value of any configuration parameter.
   */
  class geoinfo : ufw::data::base<ufw::data::complex_tag, ufw::data::unique_tag, ufw::data::global_tag> {
   public:
    class subdetector_info;
    class ecal_info;
    class grain_info;
    class tracker_info;
    class drift_info;
    class stt_info;

   private:
    class invalid_geo_id : public ufw::exception {
     public:
      invalid_geo_id(geo_id g) : ufw::exception("GUID {:x} is not a valid identifier.", g.raw) {}
    };

   public:
    explicit geoinfo(const ufw::config&);

    ~geoinfo();

    const ecal_info& ecal() const { if (!m_ecal) { init_ecal(); } return *m_ecal; }

    const grain_info& grain() const { if (!m_grain) { init_grain(); } return *m_grain; }

    const tracker_info& tracker() const { if (!m_tracker) { init_tracker(); } return *m_tracker; }

    geo_id id(const geo_path&) const;

    geo_path path(geo_id) const;

   private:
    void init_ecal() const;
    void init_grain() const;
    void init_tracker() const;

   private:
    friend class subdetector_info;
    friend class ecal_info;
    friend class grain_info;
    friend class tracker_info;
    friend class drift_info;
    friend class stt_info;

   private:
    mutable std::unique_ptr<grain_info> m_grain;
    mutable std::unique_ptr<ecal_info> m_ecal;
    mutable std::unique_ptr<tracker_info> m_tracker;
    geo_path m_root_path;
    ufw::config m_grain_cfg;
    ufw::config m_ecal_cfg;
    ufw::config m_tracker_cfg;
    subdetector_t m_tracker_type = NONE;

    const geo_path& root_path() const { return m_root_path; }

   public:

    static bool getXYLineSegmentIntersection(
      const pos_3d& v1,
      const pos_3d& v2,
      const pos_3d& p,
      const dir_3d& dir,
      pos_3d& intersection_point);

    static std::vector<pos_3d> getXYLinePolygonIntersections(
      const std::vector<pos_3d>& polygon,
      const pos_3d& line_point,
      const dir_3d& line_dir);
      
    };

} // namespace sand

UFW_DECLARE_COMPLEX_DATA(sand::geoinfo);
