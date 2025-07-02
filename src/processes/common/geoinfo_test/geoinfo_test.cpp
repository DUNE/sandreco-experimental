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
    int i = 0;
    if(gi.tracker().path().find("STT") != std::string::npos) {
      if(gi.tracker().path().find("PV") != std::string::npos) {
        const geo_path STTpath = "sand_inner_volume_PV_0/STTtracker_PV_0/CMod_02_PV_0/CMod_02_planeYY_PV_0/CMod_02_planeYY_straw_PV_5";
        UFW_INFO("Testing STT path->ID and ID->path functions using as input: '{}'", STTpath);
        auto ID_old = gi.tracker().id(STTpath); // simple test on gi.id()
        UFW_INFO("ID function test (SubdetectorID: {}; SupermoduleID: {}; PlaneID: {}; TubeID: {})", ID_old.subdetector, ID_old.stt.supermodule, ID_old.stt.plane, ID_old.stt.tube); // simple test on gi.id()
        UFW_INFO("ID path: '{}'", gi.tracker().path(ID_old));
      } else {
        const geo_path STTpath = "sand_inner_volume_0/STTtracker_0/CMod_02_0/CMod_02_planeYY_0/CMod_02_planeYY_straw_0#5";
        UFW_INFO("Testing STT path->ID and ID->path functions using as input: '{}'", STTpath);
        auto ID = gi.tracker().id(STTpath); // simple test on gi.id()
        UFW_INFO("ID function test (SubdetectorID: {}; SupermoduleID: {}; PlaneID: {}; TubeID: {})", ID.subdetector, ID.stt.supermodule, ID.stt.plane, ID.stt.tube); // simple test on gi.id()  
        UFW_INFO("ID path: '{}'", gi.tracker().path(ID));  
      }   

      for (const auto& s : gi.tracker().stations()) {
        auto nhor = s->select([](auto& w){ return std::fmod(w.angle(), M_PI) < 1e-3; }).size();
        auto nver = s->select([](auto& w){ return !std::fmod(w.angle(), M_PI) < 1e-3 && std::fmod(w.angle(), M_PI_2) < 1e-3; }).size();
        UFW_INFO("Station {}:\n - corners: [{}, {}, {}, {}];\n - {} horizontal and {} vertical wires;\n - target material {}", i++, s->top_north, s->top_south, s->bottom_north, s->bottom_south, nhor, nver, s->target);
      }
    } else {
      for (const auto& s : gi.tracker().stations()) {
        auto nhor = s->select([](auto& w){ return std::fmod(w.angle(), M_PI) < 1e-3; }).size();
        auto nver = s->select([](auto& w){ return !std::fmod(w.angle(), M_PI) < 1e-3 && std::fmod(w.angle(), M_PI_2) < 1e-3; }).size();
        UFW_INFO("Station {}:\n - corners: [{}, {}, {}, {}];\n - {} horizontal and {} vertical wires;\n - target material {}", i++, s->top_north, s->top_south, s->bottom_north, s->bottom_south, nhor, nver, s->target);
      }
    }
  }

}

UFW_REGISTER_PROCESS(sand::common::geoinfo_test)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::common::geoinfo_test)
