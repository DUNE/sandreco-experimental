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

#include <root_tgeomanager/root_tgeomanager.hpp>

namespace sand {

  geoinfo::geoinfo(const ufw::config& cfg) {
    m_root_path = cfg.value("basepath", "/volWorld_PV/rockBox_lv_PV_0/volDetEnclosure_PV_0/volSAND_PV_0/MagIntVol_volume_PV_0/");
    auto& tgm = ufw::context::current()->instance<root_tgeomanager>();
    m_grain.reset(new grain_info(*this));
    m_ecal.reset(new ecal_info(*this));
    auto nav = tgm.navigator();
    try {
      nav->cd(m_root_path / "sand_inner_volume_PV_0/STTtracker_PV_0");
      m_tracker.reset(new stt_info(*this));
    } catch (ufw::exception& e) {
      m_tracker.reset(new drift_info(*this));
    }
  }

  geoinfo::~geoinfo() = default;

  geo_id geoinfo::id(const geo_path& gp) const {
    geo_id gi;
    return gi;
  }

  geo_path geoinfo::path(geo_id gi) const {
    geo_path gp(m_root_path);
    switch (gi.subdetector) {
    case DRIFT:
    case STT:
      gp /= m_tracker->path(gi);
      break;
    case ECAL:
      gp /= m_ecal->path(gi);
      break;
    case GRAIN:
      gp /= m_grain->path(gi);
      break;
    case MUON:
    case NONE:
    default:
      UFW_ERROR("Subdetector '{}' not supported.", gi.subdetector);
    }
    return gp;
  }

}
