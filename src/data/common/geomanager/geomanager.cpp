#include <ufw/data.hpp>
#include <ufw/config.hpp>
#include <ufw/context.hpp>

#include <common/sand.h>

#include <geomanager.hpp>
#include <ecal_manager.hpp>
#include <grain_manager.hpp>
#include <tracker_manager.hpp>
#include <drift_manager.hpp>
#include <stt_manager.hpp>

#include <TFile.h>
#include <TGeoManager.h>

namespace sand {

  geomanager::path& geomanager::path::operator /= (const std::string_view& sv) {
    if (empty()) {
      assign(sv);
      return *this;
    }
    if (sv.empty()) {
      return *this;
    }
    if (back() == '/') {
      if (sv.front() == '/') {
        append(sv.substr(1));
      } else {
        append(sv);
      }
    } else {
      if (sv.front() == '/') {
        append(sv);
      } else {
        reserve(size() + 1 + sv.size());
        push_back('/');
        append(sv);
      }
    }
    return *this;
  }

  geomanager::geomanager(const ufw::config& cfg) {
    m_root_path = cfg.value("basepath", "volWorld_PV/rockBox_lv_PV_0/volDetEnclosure_PV_0/volSAND_PV_0/MagIntVol_volume_PV_0/sand_inner_volume_PV_0");
    auto filepath = cfg.path_at("geometry");
    TGeoManager::Import(filepath.c_str());

    TGeoManager* tgeo = gGeoManager;
    m_grain.reset(new grain_manager(*this, tgeo));
  }

  geomanager::~geomanager() = default;

}
