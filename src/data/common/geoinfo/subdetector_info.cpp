#include <ufw/context.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>
#include <subdetector_info.hpp>

#include <regex>

namespace sand {

  geoinfo::subdetector_info::subdetector_info(const geoinfo& gi, const geo_path& subpath) : r_info(gi), m_path(subpath) {
    auto& tgm = ufw::context::current()->instance<root_tgeomanager>();
    auto nav = tgm.navigator();
    try{
        int prev = gErrorIgnoreLevel;
        gErrorIgnoreLevel = kFatal; // suppress ROOT errors
        auto sub_path = r_info.root_path() / subpath;
        bool ok = nav->TGeoNavigator::cd(sub_path.c_str());
        gErrorIgnoreLevel = prev;
        if (!ok) {
          throw 0;
        }
    } catch (...) {
        std::regex pattern("(_0)");
        m_path = std::regex_replace(m_path, pattern, "_PV$1");
        nav->cd(r_info.root_path() / m_path);
    }

    try{
      nav->cd(r_info.root_path() / m_path);
    } catch (const ufw::exception&){
      UFW_EXCEPT(path_not_found, r_info.root_path() / m_path);
    }

    UFW_DEBUG("Using subdetector path '{}'.", m_path.c_str());
    m_centre = nav->to_master(pos_3d{0.0, 0.0, 0.0});
  }

  geoinfo::subdetector_info::~subdetector_info() = default;

}
