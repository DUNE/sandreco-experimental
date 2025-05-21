#include <ufw/context.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>
#include <subdetector_info.hpp>

namespace sand {

  geoinfo::subdetector_info::subdetector_info(const geoinfo& gi, const geo_path& subpath) : r_info(gi) {
    auto& tgm = ufw::context::current()->instance<root_tgeomanager>();
    auto nav = tgm.navigator();
    nav->cd(r_info.root_path() / subpath);
    m_centre = nav->to_master({0.0, 0.0, 0.0});
  }

}
