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

template <> struct fmt::formatter<sand::dir_3d>: formatter<string_view> {
  auto format(sand::dir_3d c, format_context& ctx) const -> format_context::iterator {
    return fmt::format_to(ctx.out(), "({:.2f}, {:.2f}, {:.2f})", c.x(), c.y(), c.z());
  }
};

template <> struct fmt::formatter<sand::grain::size_3d>: formatter<string_view> {
  auto format(sand::grain::size_3d c, format_context& ctx) const -> format_context::iterator {
    return fmt::format_to(ctx.out(), "({}, {}, {})", c.x(), c.y(), c.z());
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

  private:
    geo_path m_test_path;
    
  };

  void geoinfo_test::configure (const ufw::config& cfg) {
    process::configure(cfg);
    m_test_path = std::string(cfg.at("test_path"));
    UFW_INFO("Configuring geoinfo_test at {}.", fmt::ptr(this));
  }

  geoinfo_test::geoinfo_test() : process({}, {}) {
    UFW_INFO("Creating a geoinfo_test process at {}.", fmt::ptr(this));
  }

  void geoinfo_test::run() {
    const auto& gi = instance<geoinfo>();
    UFW_INFO("Running a geoinfo_test process at {}.", fmt::ptr(this));
    UFW_INFO("GRAIN path: '{}'", gi.grain().path());
    UFW_INFO("GRAIN position: '{}'", gi.grain().transform());
    UFW_INFO("GRAIN size (local bbox):\n - outer vessel {};\n - LAr {};\n - optics fiducial {};", gi.grain().vessel_bbox(), gi.grain().LAr_bbox(), gi.grain().fiducial_bbox());
    dir_3d sz(15., 15., 500.);
    auto voxels = gi.grain().fiducial_voxels(sz);
    std::string ascii_grain;
    for (size_t z = 0; z != voxels.size().z(); ++z) {
      for (size_t y = 0; y != voxels.size().y(); ++y) {
        for (size_t x = 0; x != voxels.size().x(); ++x) {
          ascii_grain += voxels.at(grain::index_3d(x, y, z)) ? '#' : ' ';
        }
        ascii_grain += '\n';
      }
      ascii_grain += '\n';
      ascii_grain += '\n';
    }
    UFW_INFO("GRAIN was segmented in a fiducial of {} voxels:\n{}", voxels.size(), ascii_grain);
    for (const auto& cam : gi.grain().mask_cameras()) {
      auto cam2glob = gi.grain().transform() * cam.transform;
      auto centre = cam2glob * pos_3d{0., 0., 0.};
      auto aim = cam2glob * dir_3d{0., 0., 1.};
      UFW_INFO("Camera {} [{}]:\n - centre: {};\n - view direction: {};\n - optics type: {}", cam.name, cam.id, centre, aim, cam.optics);
    }
    auto pix_spam = gi.grain().mask_cameras().front();
    UFW_INFO("First camera details:");
    for (int i = 0; i != 32; ++i) {
      for (int j = 0; j != 32; ++j) {
        UFW_INFO("SiPM rect top left = ({}, {}), bottom right = ({}, {})", pix_spam.sipm_active_areas[i][j].left, pix_spam.sipm_active_areas[i][j].top, pix_spam.sipm_active_areas[i][j].right, pix_spam.sipm_active_areas[i][j].bottom);
      }
    }
    std::for_each(pix_spam.holes.begin(), pix_spam.holes.end(), [](auto r) {
      UFW_INFO("Hole rect top left = ({}, {}), bottom right = ({}, {})", r.left, r.top, r.right, r.bottom);
    } );
    UFW_INFO("ECAL path: '{}'", gi.ecal().path());
    UFW_INFO("ECAL position: '{}'", gi.ecal().transform());
    UFW_INFO("TRACKER path: '{}'", gi.tracker().path());

    if(!m_test_path.empty()) {
      UFW_INFO("Testing Tracker path->ID and ID->path functions using as input: '{}'", m_test_path);
      auto ID = gi.tracker().id(m_test_path); 
      UFW_INFO("ID function test (SubdetectorID: {}; SupermoduleID: {}; PlaneID: {}; TubeID: {})", ID.subdetector, ID.stt.supermodule, ID.stt.plane, ID.stt.tube);
      UFW_INFO("ID path: '{}'", gi.tracker().path(ID));
    } else {
      UFW_INFO("No test path provided, skipping path->ID and ID->path tests.");
    }
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
