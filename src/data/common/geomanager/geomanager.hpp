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

  public:
    explicit geomanager(const ufw::config&);

    ~geomanager();

    const ecal_manager& ecal() const { return *m_ecal; }

    const grain_manager& grain() const { return *m_grain; }

    const tracker_manager& tracker() const { return *m_tracker; }

  private:
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
