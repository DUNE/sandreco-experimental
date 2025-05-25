#include <ufw/context.hpp>
#include <grain_info.hpp>
#include <geant_gdml_parser/geant_gdml_parser.hpp>

namespace sand {

  namespace {

    void print_auxlist(const G4GDMLAuxListType& list, std::string depth = "") {
      for (const auto& item: list) {
        UFW_DEBUG("{} Auxiliary '{}' = {} {}.", depth, item.type, item.value, item.unit);
        if (item.auxList) {
          print_auxlist(*item.auxList, depth += ' ');
        }
      }
    };

    void print_geom(const G4VPhysicalVolume* pv, std::string depth = "") {
      if (!pv) {
        return;
      }
      auto rot = pv->GetObjectRotationValue();
      auto tran = pv->GetObjectTranslation ();
      UFW_DEBUG("{}- {} (PV) is at [{}, {}, {}], [[{}, {}, {}], [{}, {}, {}], [{}, {}, {}]]", depth, pv->GetName(), tran.x(), tran.y(), tran.z(), 
                rot[0][0], rot[0][1], rot[0][2], rot[1][0], rot[1][1], rot[1][2], rot[2][0], rot[2][1], rot[2][2]);
      auto lv = pv->GetLogicalVolume();
      auto solid = lv->GetSolid();
      UFW_DEBUG("{}- {} (LV) has solid of type {}.", depth, lv->GetName(), solid->GetEntityType());
      depth += ' ';
      for (int i = 0; i != lv->GetNoDaughters(); ++i) {
        print_geom(lv->GetDaughter(i), depth);
      }
    }

    const G4VPhysicalVolume* find_by_name(const G4VPhysicalVolume* pv, std::string_view name) {
      auto lv = pv->GetLogicalVolume();
      for (int i = 0; i != lv->GetNoDaughters(); ++i) {
        if (lv->GetDaughter(i)->GetName() == name) {
          return lv->GetDaughter(i);
        }
      }
      UFW_ERROR("No volume named '{}' was found.", name);
    }

  }

  static constexpr char s_grain_path[] = "sand_inner_volume_PV_0/GRAIN_lv_PV_0/GRAIN_LAr_lv_PV_0";

  geoinfo::grain_info::grain_info(const geoinfo& gi, const std::string& inner_geom) : subdetector_info(gi, s_grain_path) {
    UFW_INFO("Reading grain geometry details from {}.", inner_geom);
    auto& gdml = ufw::context::current()->instance<grain::geant_gdml_parser>(ufw::public_id(inner_geom));
    auto world = gdml.GetWorldVolume();
    auto vessel_ext_physical = find_by_name(world, "vessel_ext_physical");
    auto air_physical = find_by_name(vessel_ext_physical, "air_physical");
    auto vessel_int_physical = find_by_name(air_physical, "vessel_int_physical");
    auto lar_physical = find_by_name(vessel_int_physical, "lar_physical");
    auto lar_logical = lar_physical->GetLogicalVolume();
    m_mask_cameras.reserve(lar_logical->GetNoDaughters());
    for (int i = 0; i != lar_logical->GetNoDaughters(); ++i) {
      auto camera = lar_logical->GetDaughter(i);
      if (camera->GetLogicalVolume()->GetName() != "cam_volume") {
        continue;
      }
      auto rot = camera->GetObjectRotationValue();
      auto tran = camera->GetObjectTranslation ();
      UFW_DEBUG("{} (PV) is at [{:.3f}, {:.3f}, {:.3f}], [[{:.3f}, {:.3f}, {:.3f}], [{:.3f}, {:.3f}, {:.3f}], [{:.3f}, {:.3f}, {:.3f}]]", camera->GetName(), tran.x(), tran.y(), tran.z(), 
                rot[0][0], rot[0][1], rot[0][2], rot[1][0], rot[1][1], rot[1][2], rot[2][0], rot[2][1], rot[2][2]);
      mask_camera mc{uint8_t(i), uint8_t(grain::mask), xform_3d(), grain::pixel_array<rect_f>{}, 0.0, 0.0, rect_f{}, std::array<rect_f, grain::camera_width * grain::camera_height / 2>{}};
      m_mask_cameras.emplace_back(mc);
    }
    UFW_DEBUG("Printing auxmap");
    for (const auto& [vol, list]: *gdml.GetAuxMap()) {
      UFW_DEBUG("Logical volume '{}' at {} has {} auxiliary info.", vol->GetName(), fmt::ptr(vol), list.size());
      print_auxlist(list);
    }
    UFW_DEBUG("End of auxmap");
    UFW_FATAL("break");
  }

  geoinfo::grain_info::~grain_info() = default;

  geo_id geoinfo::grain_info::id(const geo_path& gp) const {
    geo_id gi;
    if (gp == path()) {
      gi.subdetector = GRAIN;
    } else {
      UFW_ERROR("Incorrect path for GRAIN, expected '{}', got '{}'.", path(), gp);
    }
    return gi;
  }

  geo_path geoinfo::grain_info::path(geo_id gi) const {
    UFW_ASSERT(gi.subdetector == GRAIN, "Subdetector must be GRAIN");
    return path();
  }

}

/*
[2025-05-22 00:14:02.425] [debug] [grain_info.cpp:25] Printing auxmap
[2025-05-22 00:14:02.425] [debug] [grain_info.cpp:27] Logical volume 'codedApertureMask' at 0x1677b1a0 has 1 auxiliary info.
[2025-05-22 00:14:02.425] [debug] [grain_info.cpp:11]  Auxiliary 'Mask' = codedApertureMask .
[2025-05-22 00:14:02.425] [debug] [grain_info.cpp:11]   Auxiliary 'rank' = 31 .
[2025-05-22 00:14:02.425] [debug] [grain_info.cpp:11]   Auxiliary 'cellcount' = 61 .
[2025-05-22 00:14:02.425] [debug] [grain_info.cpp:11]   Auxiliary 'cellsize' = 2.710 mm.
[2025-05-22 00:14:02.425] [debug] [grain_info.cpp:11]   Auxiliary 'celledge' = 0.200 mm.
[2025-05-22 00:14:02.425] [debug] [grain_info.cpp:27] Logical volume 'cam_volume' at 0x167af8a0 has 1 auxiliary info.
[2025-05-22 00:14:02.425] [debug] [grain_info.cpp:11]  Auxiliary 'Camera' =  .
[2025-05-22 00:14:02.425] [debug] [grain_info.cpp:27] Logical volume 'lar_volume' at 0x16877580 has 1 auxiliary info.
[2025-05-22 00:14:02.425] [debug] [grain_info.cpp:11]  Auxiliary 'Fiducial' =  .
[2025-05-22 00:14:02.425] [debug] [grain_info.cpp:27] Logical volume 'photoDetector' at 0x168d6dc0 has 1 auxiliary info.
[2025-05-22 00:14:02.425] [debug] [grain_info.cpp:11]  Auxiliary 'Sensor' = S14160-6050HS .
[2025-05-22 00:14:02.425] [debug] [grain_info.cpp:11]   Auxiliary 'cellcount' = 32 .
[2025-05-22 00:14:02.425] [debug] [grain_info.cpp:11]   Auxiliary 'cellsize' = 3.000 mm.
[2025-05-22 00:14:02.425] [debug] [grain_info.cpp:11]   Auxiliary 'celledge' = 0.200 mm.
[2025-05-22 00:14:02.425] [critical] [grain_info.cpp:30] End of auxmap
*/
