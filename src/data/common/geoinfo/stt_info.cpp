#include <stt_info.hpp>
#include <ufw/context.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>

namespace sand {

  geoinfo::stt_info::stt_info(const geoinfo& gi) : tracker_info(gi, "sand_inner_volume_PV_0/STTtracker_PV_0") {
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

}
