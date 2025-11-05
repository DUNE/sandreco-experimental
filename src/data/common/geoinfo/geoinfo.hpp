#pragma once

#include <ufw/data.hpp>
#include <common/sand.h>

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
    class path_not_found : public ufw::exception {
    public:
      path_not_found(const geo_path& p) : exception("Cannot find path '{}' in geometry.", p.c_str()) {}
    };

    class invalid_geo_id : public ufw::exception {
    public:
      invalid_geo_id(geo_id g) : exception("GUID {:x} is not a valid identifier.", g.raw) {}
    };

    class invalid_position : public ufw::exception {
    public:
      invalid_position(pos_3d pos) : exception("No object found at position ({}, {}, {}).", pos.x(), pos.y(), pos.z()) {}
      invalid_position(std::string_view type, pos_3d pos) : exception("No {} found at position ({}, {}, {}).", type, pos.x(), pos.y(), pos.z()) {}
    };

  public:
    explicit geoinfo(const ufw::config&);

    ~geoinfo();

    const ecal_info& ecal() const { return *m_ecal; }

    const grain_info& grain() const { return *m_grain; }

    const tracker_info& tracker() const { return *m_tracker; }

    geo_id id(const geo_path&) const;

    geo_path path(geo_id) const;

  private:
    friend class subdetector_info;
    friend class ecal_info;
    friend class grain_info;
    friend class tracker_info;
    friend class drift_info;
    friend class stt_info;

  private:
    std::unique_ptr<grain_info> m_grain;
    std::unique_ptr<ecal_info> m_ecal;
    std::unique_ptr<tracker_info> m_tracker;
    geo_path m_root_path;
    geo_path m_edep_root_path;

    const geo_path& root_path() const { return m_root_path; }
    const geo_path& edep_root_path() const { return m_edep_root_path; }

  };

}

UFW_DECLARE_COMPLEX_DATA(sand::geoinfo);
