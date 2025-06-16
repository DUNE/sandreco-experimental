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

  geoinfo::stt_info::~stt_info() = default;

  geo_id geoinfo::stt_info::id(const geo_path& gp) const {
    geo_id gi;
    auto path = gp - subdetector_info::path();
    gi.subdetector = STT;
    //abuse the bad notation here, module/plane/straw
    std::string straw(path.token(2));
    auto i1 = straw.find('_');
    auto i2 = straw.find('_', i1 + 1);
    auto i3 = straw.find('_', i2 + 1);
    auto i4 = straw.find('_', i3 + 1);
    auto i5 = straw.find('_', i4 + 1);
    if (i5 != std::string::npos) {
      gi.stt.supermodule = std::stoi(straw.substr(i1, i2 - i1 - 1));
      gi.stt.plane = 0;
      if (straw.at(i3 - 1) == 'Y') {
        gi.stt.plane = 1;
      } else if (path.token(1).back() == '1') {
        gi.stt.plane = 2;
      }
      gi.stt.tube = std::stoi(straw.substr(i5));
    } else {
      UFW_ERROR("Path '{}' is incorrectly formatted for STT.", gp);
    }
    return gi;
  }

  geo_path geoinfo::stt_info::path(geo_id gi) const {
    //TODO these path names are quite poor choices, heavy repetitions etc... they should be changed in gegede
    UFW_ASSERT(gi.subdetector == STT, "Subdetector must be STT");
    geo_path gp = path();
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
