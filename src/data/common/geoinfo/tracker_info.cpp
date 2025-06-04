#include <tracker_info.hpp>

namespace sand {

  /**
   * Calculates the y-offset correction due to wire sag from a @p measured point what is referenced to this wire.
   * The correction is applied as a compensation i.e. the return value is vertically higher than the input.
   * 
   * @returns The corrected position of the point in global coordinates
   */
  pos_3d geoinfo::tracker_info::wire::actual(pos_3d measured) const {
    auto seg = segment(measured);
    catenary cat = catenaries[seg];
    auto real = measured;
    auto yoff = cat.a * std::cosh((measured.x() - cat.minimum.x()) / cat.a) - cat.a;
    real.SetY(measured.y() + yoff);
    return real;
  }

  /**
   * @returns the index of the segment of wire that this point is closest to, guaranteed to be <= s_max_wire_spacers
   */
  std::size_t geoinfo::tracker_info::wire::segment(pos_3d x) const {
    double t = x.x() - std::min(head.x(), tail.x());
    std::size_t i = 0;
    while (i != s_max_wire_spacers && spacers[i] > t) {
      ++i;
    }
    return i;
  }

  geoinfo::tracker_info::tracker_info(const geoinfo& gi, const geo_path& p) : subdetector_info(gi, p) {}

  void geoinfo::tracker_info::add_plane(plane_ptr&& p) {
    m_planes.emplace_back(std::move(p));
  }

  void geoinfo::tracker_info::add_volume(const geo_path& p, const gas_volume& v) {
    m_volumes[p] = v;
  }

}
