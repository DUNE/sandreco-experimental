#pragma once

#include <ufw/data.hpp>

namespace sand {

  class geomanager : ufw::data::base<ufw::data::complex_tag, ufw::data::unique_tag, ufw::data::global_tag> {

  public:
    class ecal_manager;
    class grain_manager;
    class tracker_manager;
    class drift_manager;
    class stt_manager;

  public:
    explicit geomanager(const ufw::config&);

    ~geomanager();

    const ecal_manager& ecal() const { return *m_ecal; }

    const grain_manager& grain() const { return *m_grain; }

    const tracker_manager& tracker() const { return *m_tracker; }

  private:
    std::unique_ptr<grain_manager> m_grain;
    std::unique_ptr<ecal_manager> m_ecal;
    std::unique_ptr<tracker_manager> m_tracker;

  };

}

UFW_DECLARE_COMPLEX_DATA(sand::geomanager);
