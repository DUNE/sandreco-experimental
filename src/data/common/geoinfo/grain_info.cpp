#include <grain_info.hpp>

namespace sand {

  static constexpr char s_grain_path[] = "sand_inner_volume_PV_0/GRAIN_lv_PV_0/GRAIN_LAr_lv_PV_0";

  geoinfo::grain_info::grain_info(const geoinfo& gi) : subdetector_info(gi, s_grain_path) {}

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
