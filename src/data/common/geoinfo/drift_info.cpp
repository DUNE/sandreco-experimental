#include <drift_info.hpp>
#include <ufw/context.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>

#include <TGeoMatrix.h>
#include <TGeoBBox.h>
#include <TGeoTube.h>

namespace sand {

  static constexpr char s_drift_path[] = "sand_inner_volume_0/SANDtracker_0";

  geoinfo::drift_info::drift_info(const geoinfo& gi) : tracker_info(gi, "sand_inner_volume_0/") {
    //UFW_FATAL("drift");
    auto& tgm = ufw::context::current()->instance<root_tgeomanager>();
    auto nav = tgm.navigator();
    auto driftpath =  gi.root_path() / path();

    nav->cd(driftpath);
    nav->for_each_node([&](auto supermod) {
      std::string smodname = supermod->GetName();
      nav->cd(driftpath / smodname);
      nav->for_each_node([&](auto mod) {
        std::string modname = mod->GetName();
        nav->cd(driftpath / smodname / modname);
        auto stat = std::make_unique<station>();
        target_material tgt;
        if (smodname.find("C3H6Mod_") == 0) {
          tgt = C3H6;
        } else if (smodname.find("CMod_") == 0) {
          tgt = CARBON;
        } else {
          UFW_ERROR("Drift module '{}' has unrecognized material.", modname);
        }
        stat->target = tgt;
        TGeoBBox* plane_shape = dynamic_cast<TGeoBBox*>(mod->GetVolume()->GetShape());
        if (!plane_shape) {
          UFW_ERROR("Drift module '{}' has invalid shape.", modname);
        }
      } );
    } );
  }

  geoinfo::drift_info::~drift_info() = default;

  geo_id geoinfo::drift_info::id(const geo_path&) const {
    geo_id gi;
    return gi;
  }

  geo_path geoinfo::drift_info::path(geo_id) const {
    geo_path gp;
    return gp;
  }

}
