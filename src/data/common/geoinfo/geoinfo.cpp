#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/data.hpp>

#include <common/sand.h>

#include <drift_info.hpp>
#include <ecal_info.hpp>
#include <geoinfo.hpp>
#include <grain_info.hpp>
#include <stt_info.hpp>
#include <tracker_info.hpp>

#include <TFile.h>
#include <TGeoNavigator.h>
#include <regex>

#include <root_tgeomanager/root_tgeomanager.hpp>

namespace sand {

  geoinfo::geoinfo(const ufw::config& cfg) {
    m_root_path      = cfg.value("basepath", "/volWorld_PV_1/rockBox_lv_PV_0/volDetEnclosure_PV_0/volSAND_PV_0/MagIntVol_volume_PV_0/");
    auto& tgm = ufw::context::current()->instance<root_tgeomanager>();

    auto grain_path = cfg.at("grain_geometry");

    auto nav               = tgm.navigator();

    try {
      nav->cd(m_root_path.c_str());
    } catch (ufw::exception& e) {
      UFW_EXCEPT(path_not_found, m_root_path);
    }

    UFW_DEBUG("Using root path '{}'.", m_root_path.c_str());

    m_grain.reset(new grain_info(*this, grain_path));
    m_ecal.reset(new ecal_info(*this));

    auto subpath = m_root_path;
    subpath = m_root_path / "sand_inner_volume_PV_0";

    nav->cd(subpath.c_str());
    bool isSTT = false;
    for (int d = 0; d < nav->GetCurrentNode()->GetNdaughters(); ++d) {
      std::string daughter_tmp = nav->GetCurrentNode()->GetDaughter(d)->GetName(); // STTtracker_PV_0
      if (daughter_tmp.find("STTtracker") != std::string::npos) {
        isSTT = true;
        break;
      }
    }

    if (isSTT) {
      UFW_INFO("STT subdetector implementation detected.");
      m_tracker.reset(new stt_info(*this));
    } else {
      UFW_INFO("Drift subdetector implementation detected.");
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

} // namespace sand
