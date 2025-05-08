#pragma once

#include <ufw/data.hpp>
#include <common/sand.h>

class TGeoManager;

namespace sand {

  /**
   * Implementation guide: all members that belong to a specific subdetector go in its _manager class.
   * Keep only general purpose members in the main class, and keep the value of any configuration parameter.
   */
  class geomanager : ufw::data::base<ufw::data::complex_tag, ufw::data::unique_tag, ufw::data::global_tag> {

  public:
    /**
     * A global unique identifier for all active elements in the detector
     */
    class guid {

    public:
      uint64_t raw() const { return m_data.m_raw; }

    private:
      union {
        uint64_t m_raw;
        struct { uint16_t m_detid; uint16_t m_modid; uint32_t m_chid; } m_daq_id;
      } m_data;

    };

    class subdetector_manager;
    class ecal_manager;
    class grain_manager;
    class tracker_manager;
    class drift_manager;
    class stt_manager;

  private:
    class path : public std::string {
    public:
      using std::string::string;
      using std::string::operator=;
      path& operator /= (const std::string_view&);
      path operator / (const std::string_view& rhs) const { path p(*this); return p /= rhs; }
      std::string_view token(std::size_t) const;
    };

    class path_not_found : public ufw::exception {
    public:
      path_not_found(const path& p) : exception("Cannot find path '{}' in geometry.", p.c_str()) {}
    };

    class invalid_guid : public ufw::exception {
    public:
      invalid_guid(guid g) : exception("GUID {:x} is not a valid identifier.", g.raw()) {}
    };

    class invalid_position : public ufw::exception {
    public:
      invalid_position(pos_3d pos) : exception("No object found at position ({}, {}, {}).", pos.x(), pos.y(), pos.z()) {}
      invalid_position(std::string_view type, pos_3d pos) : exception("No {} found at position ({}, {}, {}).", type, pos.x(), pos.y(), pos.z()) {}
    };

  public:
    explicit geomanager(const ufw::config&);

    ~geomanager();

    const ecal_manager& ecal() const { return *m_ecal; }

    const grain_manager& grain() const { return *m_grain; }

    const tracker_manager& tracker() const { return *m_tracker; }

  private:
    friend class subdetector_manager;
    friend class ecal_manager;
    friend class grain_manager;
    friend class tracker_manager;
    friend class drift_manager;
    friend class stt_manager;

    const path& root_path() const { return m_root_path; }

    TGeoManager* root_gm() const { return m_root_gm; }

  private:
    std::unique_ptr<grain_manager> m_grain;
    std::unique_ptr<ecal_manager> m_ecal;
    std::unique_ptr<tracker_manager> m_tracker;
    path m_root_path;
    TGeoManager* m_root_gm;

  };

}

UFW_DECLARE_COMPLEX_DATA(sand::geomanager);
