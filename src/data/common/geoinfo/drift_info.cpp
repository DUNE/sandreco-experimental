#include <drift_info.hpp>

namespace sand {

  geoinfo::drift_info::drift_info(const geoinfo& gi) : tracker_info(gi, "sand_inner_volume_PV_0/") {
    UFW_FATAL("drift");
  }

  geoinfo::drift_info::~drift_info() = default;

  geo_id geoinfo::drift_info::id(const geo_path&) const {
    geo_id gi;
    return gi;
  }

  geo_path geoinfo::drift_info::path(geo_id) const {
    geo_path gp;
    return gp;
  }

}
