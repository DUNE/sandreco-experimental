#include <ecal_info.hpp>

namespace sand {

  geoinfo::ecal_info::ecal_info(const geoinfo& gi) : subdetector_info(gi, "kloe_calo_volume_PV_0") {}

  geoinfo::ecal_info::~ecal_info() = default;

  geo_id geoinfo::ecal_info::id(const geo_path&) const {
    geo_id gi;
    return gi;
  }

  geo_path geoinfo::ecal_info::path(geo_id) const {
    geo_path gp;
    return gp;
  }

} // namespace sand
