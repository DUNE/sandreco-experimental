#include <ufw/data.hpp>
#include <ufw/config.hpp>
#include <ufw/context.hpp>

#include <common/sand.h>

#include <geoinfo.hpp>
#include <ecal_info.hpp>
#include <grain_info.hpp>
#include <tracker_info.hpp>
#include <drift_info.hpp>
#include <stt_info.hpp>

#include <TFile.h>

namespace sand {

  geoinfo::geoinfo(const ufw::config& cfg) {
    m_root_path = cfg.value("basepath", "/volWorld_PV/rockBox_lv_PV_0/volDetEnclosure_PV_0/volSAND_PV_0/MagIntVol_volume_PV_0/");
    m_grain.reset(new grain_info(*this));
    m_ecal.reset(new ecal_info(*this));
    m_tracker.reset(new drift_info(*this));
    m_tracker.reset(new stt_info(*this));
  }

  geoinfo::~geoinfo() = default;

}
