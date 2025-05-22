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

  }

  static constexpr char s_grain_path[] = "sand_inner_volume_PV_0/GRAIN_lv_PV_0/GRAIN_LAr_lv_PV_0";

  geoinfo::grain_info::grain_info(const geoinfo& gi, const std::string& inner_geom) : subdetector_info(gi, s_grain_path) {
    UFW_INFO("Reading grain geometry details from {}.", inner_geom);
    auto& gdml = ufw::context::current()->instance<grain::geant_gdml_parser>(ufw::public_id(inner_geom));
    UFW_DEBUG("Printing auxmap");
    for (const auto& [vol, list]: *gdml.GetAuxMap()) {
      UFW_DEBUG("Logical volume '{}' at {} has {} auxiliary info.", vol->GetName(), fmt::ptr(vol), list.size());
      print_auxlist(list);
    }
    UFW_DEBUG("End of auxmap");
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
