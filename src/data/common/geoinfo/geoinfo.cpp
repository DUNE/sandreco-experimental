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
#include <TGeoNavigator.h>
#include <regex>

#include <root_tgeomanager/root_tgeomanager.hpp>

namespace sand {

  geoinfo::geoinfo(const ufw::config& cfg) {

    m_root_path = cfg.value("basepath", "/volWorld/rockBox_lv_0/volDetEnclosure_0/volSAND_0/MagIntVol_volume_0/");
    auto& tgm = ufw::context::current()->instance<root_tgeomanager>();
    auto nav = tgm.navigator();
    std::string world_path = nav->GetPath();
    bool PV_needed = world_path.find("_PV") != std::string::npos;

    if(PV_needed){
      // Step 1: Add _PV before _0
      std::regex pattern_with_0("(_0)");
      m_root_path = std::regex_replace(m_root_path, pattern_with_0, "_PV$1");

      // Step 2: Add _PV after World
      std::regex pattern_without_0("(volWorld)");
      m_root_path = std::regex_replace(m_root_path, pattern_without_0, "$1_PV");
    }

    try{
      nav->cd(m_root_path.c_str());
    } catch (ufw::exception& e) {
      UFW_EXCEPT(path_not_found, m_root_path);
    }

    UFW_DEBUG("Using root path '{}'.", m_root_path.c_str());


    m_grain.reset(new grain_info(*this));
    m_ecal.reset(new ecal_info(*this));

    auto subpath = m_root_path;
    
    if(PV_needed){
      subpath = m_root_path / "sand_inner_volume_PV_0";
    } else {
      subpath = m_root_path / "sand_inner_volume_0";
    }

    nav->cd(subpath.c_str());
    bool isSTT = false;
    for(int d = 0; d < nav->GetCurrentNode()->GetNdaughters(); ++d){
      std::string daughter_tmp = nav->GetCurrentNode()->GetDaughter(d)->GetName(); // STTtracker_PV_0
      if(daughter_tmp.find("STTtracker") != std::string::npos){
        isSTT = true;
        break;
      }
    }

    if(isSTT){
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

  geo_path geoinfo::partial_path(const geo_path& full_path) const {
    return full_path - m_root_path;
  }

}
