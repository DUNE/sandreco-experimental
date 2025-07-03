#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <common/sand.h>
#include <geoinfo/geoinfo.hpp>
#include <geoinfo/grain_info.hpp>
#include <geoinfo/ecal_info.hpp>
#include <geoinfo/tracker_info.hpp>

template <> struct fmt::formatter<sand::pos_3d>: formatter<string_view> {
  auto format(sand::pos_3d c, format_context& ctx) const -> format_context::iterator {
    return fmt::format_to(ctx.out(), "({:.2f}, {:.2f}, {:.2f})", c.x(), c.y(), c.z());
  }
};

namespace sand::common {

  class geoinfo_test : public ufw::process {

  public:
    geoinfo_test();
    void configure (const ufw::config& cfg) override;
    void run() override;
    
  };

  void geoinfo_test::configure (const ufw::config& cfg) {
    process::configure(cfg);
    UFW_INFO("Configuring geoinfo_test at {}.", fmt::ptr(this));
  }

  geoinfo_test::geoinfo_test() : process({}, {}) {
    UFW_INFO("Creating a geoinfo_test process at {}.", fmt::ptr(this));
  }

  void geoinfo_test::run() {
    const auto& gi = instance<geoinfo>();
    UFW_INFO("Running a geoinfo_test process at {}."), fmt::ptr(this);
    UFW_INFO("GRAIN path: '{}'", gi.grain().path());
    UFW_INFO("ECAL path: '{}'", gi.ecal().path());
    UFW_INFO("TRACKER path: '{}'", gi.tracker().path());


    if(gi.tracker().path().find("STT") != std::string::npos) {
      if(gi.tracker().path().find("PV") != std::string::npos) {
        const geo_path STTpath = "sand_inner_volume_PV_0/STTtracker_PV_0/CMod_02_PV_0/CMod_02_planeYY_PV_0/CMod_02_planeYY_straw_PV_5";
        UFW_INFO("Testing STT path->ID and ID->path functions using as input: '{}'", STTpath);
        auto ID = gi.tracker().id(STTpath); // simple test on gi.id()
        UFW_INFO("ID function test (SubdetectorID: {}; SupermoduleID: {}; PlaneID: {}; TubeID: {})", ID.subdetector, ID.stt.supermodule, ID.stt.plane, ID.stt.tube); // simple test on gi.id()
        UFW_INFO("ID path: '{}'", gi.tracker().path(ID));
      } else {
        const geo_path STTpath = "sand_inner_volume_0/STTtracker_0/CMod_02_0/CMod_02_planeYY_0/CMod_02_planeYY_straw_0#5";
        UFW_INFO("Testing STT path->ID and ID->path functions using as input: '{}'", STTpath);
        auto ID = gi.tracker().id(STTpath); // simple test on gi.id()
        UFW_INFO("ID function test (SubdetectorID: {}; SupermoduleID: {}; PlaneID: {}; TubeID: {})", ID.subdetector, ID.stt.supermodule, ID.stt.plane, ID.stt.tube); // simple test on gi.id()  
        UFW_INFO("ID path: '{}'", gi.tracker().path(ID));  
      }   
    } else {
      if(gi.tracker().path().find("PV") != std::string::npos) {
        const geo_path Driftpath = "sand_inner_volume_PV_0/SANDtracker_PV_0/SuperMod_B_PV_1/C3H6Mod_B_PV_2/C3H6DriftChamber_B_PV_0/C3H6DriftModule_1_B_PV_0/C3H6DriftModule_1_B_Fwire_PV_1";
        const geo_path Driftpath_trk = "sand_inner_volume_PV_0/SANDtracker_PV_0/Trk_PV_0/TrkDrift_PV_0/CDriftModule_1_PV_0/CDriftModule_1_Fwire_PV_1";
        UFW_INFO("Testing Drift path->ID and ID->path functions using as input: '{}'", Driftpath_trk);
        auto ID = gi.tracker().id(Driftpath_trk);
        UFW_INFO("ID function test (SubdetectorID: {}; SupermoduleID: {}; PlaneID: {})", ID.subdetector, ID.drift.supermodule, ID.drift.plane);
        UFW_INFO("ID path: '{}'", gi.tracker().path(ID));
      }else{
        const geo_path Driftpath = "sand_inner_volume_0/SANDtracker_0/SuperMod_B_0#1/C3H6Mod_B_0#2/C3H6DriftChamber_B_0/C3H6DriftModule_1_B_0/C3H6DriftModule_1_B_Fwire_0#1";
        const geo_path Driftpath_trk = "sand_inner_volume_0/SANDtracker_0/Trk_0/TrkDrift_0/CDriftModule_1_0/CDriftModule_1_Fwire_0#1";
        UFW_INFO("Testing Drift path->ID and ID->path functions using as input: '{}'", Driftpath);
        auto ID = gi.tracker().id(Driftpath);
        UFW_INFO("ID function test (SubdetectorID: {}; SupermoduleID: {}; PlaneID: {})", ID.subdetector, ID.drift.supermodule, ID.drift.plane);
        UFW_INFO("ID path: '{}'", gi.tracker().path(ID));
      }      
    }

    int i = 0;
    for (const auto& s : gi.tracker().stations()) {
        auto nhor = s->select([](auto& w){ return std::fmod(w.angle(), M_PI) < 1e-3; }).size();
        auto nver = s->select([](auto& w){ return !std::fmod(w.angle(), M_PI) < 1e-3 && std::fmod(w.angle(), M_PI_2) < 1e-3; }).size();
        UFW_INFO("Station {}:\n - corners: [{}, {}, {}, {}];\n - {} horizontal and {} vertical wires;\n - target material {}", i++, s->top_north, s->top_south, s->bottom_north, s->bottom_south, nhor, nver, s->target);
      }
  }

}

UFW_REGISTER_PROCESS(sand::common::geoinfo_test)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::common::geoinfo_test)
