#include <stt_info.hpp>
#include <ufw/context.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>

namespace sand {

  static constexpr char s_stt_path[] = "sand_inner_volume_PV_0/STTtracker_PV_0";

  geoinfo::stt_info::stt_info(const geoinfo& gi) : tracker_info(gi, s_stt_path) {
    auto& tgm = ufw::context::current()->instance<root_tgeomanager>();
    auto nav = tgm.navigator();
    auto path =  gi.root_path() / "sand_inner_volume_PV_0/STTtracker_PV_0/TrkMod_00_PV_0/TrkMod_00_planeXX_PV_1";
    station_ptr pl(new station);
    //pl->top_north
    nav->cd(path);
    nav->for_each_node([&](auto node) {
      UFW_INFO("Child {}", node->GetName());
      gas_volume gv;
      //gv.w = ...
      //get the transorm of this shape, find position of wire
      //then create wire, add it to station, and assign to gv.w
      gv.p = pl.get();
      gv.gas = "boh";
      gv.gas_pressure = -999.;
      add_volume(path / node->GetName(), gv);
    } );
    //UFW_FATAL("stt");
  }

  geo_id geoinfo::stt_info::id(const geo_path& gp) const {
    geo_id gi;
    
    return gi;
  }

  geo_path geoinfo::stt_info::path(geo_id gi) const {
    //TODO these path names are quite poor choices, heavy repetitions etc... they should be changed in gegede
    UFW_ASSERT(gi.subdetector == STT, "Subdetector must be STT");
    geo_path gp(s_stt_path);
    auto stat = at(gi.stt.supermodule);
    std::string module_name;
    switch (stat->target) {
    case TRKONLY:
      module_name = "TrkMod_";
      break;
    case C3H6:
      module_name = "C3H6Mod_";
      break;
    case CARBON:
      module_name = "CMod_";
      break;
    default:
      UFW_ERROR("Target material '{}' unsupported.", stat->target);
    }
    module_name += fmt::format("{:02}", gi.stt.supermodule);
    gp /= module_name + "_PV_0";
    if (gi.stt.plane == 0) {
      module_name += "_planeXX";
      gp /= module_name + "_PV_0";
    } else if (gi.stt.plane == 1) {
      module_name += "_planeXX";
      gp /= module_name + "_PV_0";
    } else if (gi.stt.plane == 1 && stat->target == TRKONLY) {
      module_name += "_planeXX";
      gp /= module_name + "_PV_1";
    } else {
      UFW_ERROR("Plane '{}' unsupported.", gi.stt.plane);
    }
    //TODO check max tube for this layer
    gp /= module_name + fmt::format("_PV_{}", gi.stt.tube);
    return gp;
  }

}
