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

template <> struct fmt::formatter<sand::xform_3d>: formatter<string_view> {
  auto format(sand::xform_3d xfrm, format_context& ctx) const -> format_context::iterator {
    double d[12];
    xfrm.GetComponents(d);
    return fmt::format_to(ctx.out(), "[{:.2f}, {:.2f}, {:.2f}], [[{:.2f}, {:.2f}, {:.2f}], [{:.2f}, {:.2f}, {:.2f}], [{:.2f}, {:.2f}, {:.2f}]]",
                                       d[3], d[7], d[11], d[0], d[1], d[2], d[4], d[5], d[6], d[8], d[9], d[10]);
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
    UFW_INFO("GRAIN position: '{}'", gi.grain().transform());
    UFW_INFO("ECAL path: '{}'", gi.ecal().path());
    UFW_INFO("ECAL position: '{}'", gi.ecal().transform());
    UFW_INFO("TRACKER path: '{}'", gi.tracker().path());
    UFW_INFO("TRACKER position: '{}'", gi.tracker().transform());
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
