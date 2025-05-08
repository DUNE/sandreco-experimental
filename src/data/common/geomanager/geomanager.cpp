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

  std::string_view geomanager::path::token(std::size_t i) const {
    std::size_t start = 0;
    std::size_t stop = 0;
     while (i--) {
       start = find('/', start + 1);
    }
    stop = find('/', start + 1);
    if (stop == std::string::npos) {
      stop = size();
    }
    return std::string_view(data() + start + 1, stop - start - 1);
  }

  geomanager::geomanager(const ufw::config& cfg) {
    m_root_path = cfg.value("basepath", "/volWorld_PV/rockBox_lv_PV_0/volDetEnclosure_PV_0/volSAND_PV_0/MagIntVol_volume_PV_0/sand_inner_volume_PV_0");
    auto filepath = cfg.path_at("geometry");
    m_root_gm = TGeoManager::Import(filepath.c_str());
    if (!m_root_gm) {
      UFW_ERROR("Cannot find valid TGeoManager in {}.", filepath.c_str());
    }
    m_grain.reset(new grain_manager(*this));
    m_ecal.reset(new ecal_manager(*this));
    m_tracker.reset(new drift_manager(*this));
    m_tracker.reset(new stt_manager(*this));
  }

  geomanager::~geomanager() = default;

}
