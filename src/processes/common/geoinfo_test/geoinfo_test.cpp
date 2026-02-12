#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <geoinfo/ecal_info.hpp>
#include <geoinfo/geoinfo.hpp>
#include <geoinfo/grain_info.hpp>
#include <geoinfo/tracker_info.hpp>
#include <common/sand.h>

namespace sand::common {

  class geoinfo_test : public ufw::process {
   public:
    geoinfo_test();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    geo_path m_test_path;
    std::string m_mask_or_lens;
  };

  void geoinfo_test::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_test_path    = std::string(cfg.at("test_path"));
    m_mask_or_lens = std::string(cfg.at("mask_or_lens"));
    UFW_INFO("Configuring geoinfo_test at {}.", fmt::ptr(this));
  }

  geoinfo_test::geoinfo_test() : process({}, {}) { UFW_INFO("Creating a geoinfo_test process at {}.", fmt::ptr(this)); }

  void geoinfo_test::run() {
    const auto& gi = instance<geoinfo>();
    UFW_INFO("Running a geoinfo_test process at {}.", fmt::ptr(this));
    UFW_INFO("GRAIN path: '{}'", gi.grain().path());
    UFW_INFO("GRAIN position: '{}'", gi.grain().transform());
    UFW_INFO("GRAIN size (local bbox):\n - LAr {};\n - optics fiducial {};", gi.grain().LAr_bbox(),
             gi.grain().fiducial_bbox());
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
    for (size_t k = 0; k != voxels.size().z() && k < 100; ++k) { // Just few cycles for testing
      for (size_t j = 0; j != voxels.size().y() && j < 100; ++j) {
        for (size_t i = 0; k != voxels.size().x() && i < 100; ++i) {
          pos_3d voxel_position{gi.grain().voxel_index_to_position(grain::index_3d(i, j, k), sz, voxels.size())};
          UFW_INFO("index: ({},{},{}) position: ({},{},{})\n", i, j, k, voxel_position.x(), voxel_position.y(),
                   voxel_position.z());
        }
      }
    }
    if (m_mask_or_lens == "mask") {
      for (const auto& cam : gi.grain().mask_cameras()) {
        auto cam2glob = gi.grain().transform() * cam.transform;
        auto centre   = cam2glob * pos_3d{0., 0., 0.};
        auto aim      = cam2glob * dir_3d{0., 0., 1.};
        UFW_INFO("Camera {} [{}]:\n - centre: {};\n - view direction: {};\n - optics type: {}", cam.name, cam.id,
                 centre, aim, cam.optics);
      }
      auto pix_spam = gi.grain().mask_cameras().front();
      UFW_INFO("First camera details:");
      for (int i = 0; i != 32; ++i) {
        for (int j = 0; j != 32; ++j) {
          UFW_INFO("SiPM rect top left = ({}, {}), bottom right = ({}, {})", pix_spam.sipm_active_areas[i][j].left,
                   pix_spam.sipm_active_areas[i][j].top, pix_spam.sipm_active_areas[i][j].right,
                   pix_spam.sipm_active_areas[i][j].bottom);
        }
      }
      std::for_each(pix_spam.holes.begin(), pix_spam.holes.end(), [](auto r) {
        UFW_INFO("Hole rect top left = ({}, {}), bottom right = ({}, {})", r.left, r.top, r.right, r.bottom);
      });
    } else if (m_mask_or_lens == "lens") {
      for (const auto& cam : gi.grain().lens_cameras()) {
        auto cam2glob = gi.grain().transform() * cam.transform;
        auto centre   = cam2glob * pos_3d{0., 0., 0.};
        auto aim      = cam2glob * dir_3d{0., 0., 1.};
        UFW_INFO("Camera {} [{}]:\n - centre: {};\n - view direction: {};\n - optics type: {}", cam.name, cam.id,
                 centre, aim, cam.optics);
      }
      auto pix_spam = gi.grain().lens_cameras().front();
      UFW_INFO("First camera details:");
      for (int i = 0; i != 32; ++i) {
        for (int j = 0; j != 32; ++j) {
          UFW_INFO("SiPM rect top left = ({}, {}), bottom right = ({}, {})", pix_spam.sipm_active_areas[i][j].left,
                   pix_spam.sipm_active_areas[i][j].top, pix_spam.sipm_active_areas[i][j].right,
                   pix_spam.sipm_active_areas[i][j].bottom);
        }
      }
      UFW_INFO("Last camera info: distance lens-sensor = {}", pix_spam.z_lens);
    }

    UFW_INFO("ECAL path: '{}'", gi.ecal().path());
    UFW_INFO("ECAL position: '{}'", gi.ecal().transform());

    for (uint8_t ism = 0; ism < 3; ism++) {
      UFW_INFO("SM: {}", ism == 0 ? "BARREL" : (ism == 1 ? "ENDCAP_A" : "ENDCAP_B"));
      for (uint8_t im = 0; im < (ism == 0 ? 24 : 32); im++) {
        UFW_INFO("  MODULE: {}", im);
        for (uint8_t ir = 0; ir < 5; ir++) {
          uint8_t nc = 12;
          if (ism != 0) {
            auto iim = uint8_t(im % 16);
            if (iim < 2)
              nc = 6;
            else if (iim > 11)
              nc = 2;
            else
              nc = 3;
          }
          for (uint8_t ic = 0; ic < nc; ic++) {
            sand::geoinfo::ecal_info::cell_id cid;
            cid.region        = sand::geo_id::region_t(ism);
            cid.module_number = im;
            cid.row           = ir;
            cid.column        = ic;
            auto& c           = gi.ecal().at(cid);
            UFW_INFO("    cell: {}, row: {}, column: {}", cid.raw, ir, ic);
            UFW_INFO("    elements size: {}", c.element_collection().elements().size());
            UFW_INFO("      face[1] centroid: {}", c.element_collection().elements().front()->begin_face().centroid());
            UFW_INFO("      face[2] centroid: {}", c.element_collection().elements().back()->end_face().centroid());
          }
        }
      }
    }

    sand::geoinfo::ecal_info::cell_id cid;
    cid.region        = sand::geo_id::region_t::BARREL;
    cid.module_number = 13;
    cid.row           = 4;
    cid.column        = 7;
    auto& cb          = gi.ecal().at(cid);
    auto c1           = cb.element_collection().elements().front()->begin_face().centroid();
    auto c2           = cb.element_collection().elements().back()->end_face().centroid();
    auto p            = c1 + 0.3 * (c2 - c1);
    auto obt_cid      = gi.ecal().at(p).id();
    auto l1exp        = (c1 - p).R();
    auto l2exp        = (c2 - p).R();
    auto lexp         = l1exp + l2exp;
    auto l1obt        = cb.pathlength(p, sand::geoinfo::ecal_info::face_location::begin);
    auto l2obt        = cb.pathlength(p, sand::geoinfo::ecal_info::face_location::end);
    auto lobt         = cb.total_pathlength();
    auto off          = -(0.5 * lexp - l1exp);
    auto pobt         = cb.offset2position(off);

    UFW_ASSERT(lexp == lobt,
               fmt::format("[ECAL BARREL] Total pathlength doesn't match!! Expected: {} - Obtained: {}", lexp, lobt));
    UFW_ASSERT(l1exp == l1obt,
               fmt::format("[ECAL BARREL] Pathlength doesn't match!! Expected: {} - Obtained: {}", l1exp, l1obt));
    UFW_ASSERT(p == pobt,
               fmt::format("[ECAL BARREL] Points don't match!!! Expected point: {} - Obtained point: {}", p, pobt));
    UFW_ASSERT(cb.is_inside(p), fmt::format("[ECAL BARREL] Point: {} is expected to be inside!!", p));
    UFW_ASSERT(cid.raw == obt_cid.raw,
               fmt::format("[ECAL BARREL] Unexpected cell id!! Provided: {} - Obtained: {}", cid.raw, obt_cid.raw));

    cid.region        = sand::geo_id::region_t::ENDCAP_A;
    cid.module_number = 0;
    cid.row           = 4;
    cid.column        = 2;
    auto& ce          = gi.ecal().at(cid);
    off               = -0.7 * 0.5 * ce.total_pathlength();
    p                 = ce.offset2position(off);
    obt_cid           = gi.ecal().at(p).id();
    l1obt             = ce.pathlength(p, sand::geoinfo::ecal_info::face_location::begin);
    l2obt             = ce.pathlength(p, sand::geoinfo::ecal_info::face_location::end);
    l1exp             = 0.5 * ce.total_pathlength() + off;
    l2exp             = 0.5 * ce.total_pathlength() - off;
    lobt              = ce.total_pathlength();
    lexp              = l1exp + l2exp;

    UFW_ASSERT(lexp == lobt,
               fmt::format("[ECAL ENDCAP] Total pathlength doesn't match!! Expected: {} - Obtained: {}", lexp, lobt));
    UFW_ASSERT(2. * (l1exp - l1obt) / (l1exp + l1obt) < 1.E-9,
               fmt::format("[ECAL ENDCAP] Pathlength doesn't match!! Expected: {} - Obtained: {}", l1exp, l1obt));
    UFW_ASSERT(ce.is_inside(p), fmt::format("[ECAL ENDCAP] Point: {} is expected to be inside!!", p));
    UFW_ASSERT(cid.raw == obt_cid.raw,
               fmt::format("[ECAL ENDCAP] Unexpected cell id!! Provided: {} - Obtained: {}", cid.raw, obt_cid.raw));

    UFW_INFO("TRACKER path: '{}'", gi.tracker().path());

    bool isSTT = (gi.tracker().path().find("STT") != std::string::npos);
    if (isSTT)
      UFW_INFO("TRACKER is STT");
    else
      UFW_INFO("TRACKER is DRIFT");

    if (!m_test_path.empty()) {
      UFW_INFO("Testing Tracker path->ID and ID->path functions using as input: '{}'", m_test_path);
      auto ID = gi.tracker().id(gi.tracker().partial_path(m_test_path, gi));
      if (isSTT) {
        UFW_INFO("ID function test (SubdetectorID: {}; SupermoduleID: {}; PlaneID: {}; TubeID: {})", ID.subdetector,
                 ID.stt.supermodule, ID.stt.plane, ID.stt.tube);
      } else {
        UFW_INFO("ID function test (SubdetectorID: {}; SupermoduleID: {}; PlaneID: {})", ID.subdetector,
                 ID.drift.supermodule, ID.drift.plane);
      }
      UFW_INFO("ID path: '{}'", gi.tracker().path(ID));
    } else {
      UFW_INFO("No test path provided, skipping path->ID and ID->path tests.");
    }

    UFW_INFO("TRACKER position: '{}'", gi.tracker().transform());

    int i = 0;
    for (const auto& s : gi.tracker().stations()) {
      if (isSTT) {
        auto nhor = s->select([](auto& w) { return std::fmod(w.angle(), M_PI) < 1e-3; }).size();
        auto nver = s->select([](auto& w) {
                       return !std::fmod(w.angle(), M_PI) < 1e-3 && std::fmod(w.angle(), M_PI_2) < 1e-3;
                     }).size();
        UFW_INFO("Station {}:\n - corners: [{}, {}, {}, {}];\n - {} h and {} v wires;\n - target material {}", i++,
                 s->top_north, s->top_south, s->bottom_north, s->bottom_south, nhor, nver, s->target);
      } else {
        auto nx = s->select([&](const auto& w) { return std::abs(w.angle() - 0.0) < 1e-6; }).size();
        auto nu = s->select([&](const auto& w) { return std::abs(w.angle() - (M_PI / 36.0)) < 1e-6; }).size();
        auto nv = s->select([&](const auto& w) { return std::abs(w.angle() + (M_PI / 36.0)) < 1e-6; }).size();
        UFW_INFO("Station {}:\n - corners: [{}, {}, {}, {}];\n - {} x, {} u, and {} v wires;\n - target material {}",
                 i++, s->top_north, s->top_south, s->bottom_north, s->bottom_south, nx, nu, nv, s->target);
      }
    }
  }
} // namespace sand::common

UFW_REGISTER_PROCESS(sand::common::geoinfo_test)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::common::geoinfo_test)
