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
    if (std::isfinite(cat.a)) {
      auto yoff = cat.a * std::cosh((measured.x() - cat.minimum.x()) / cat.a) - cat.a;
      real.SetY(measured.y() + yoff);
    }
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

  geoinfo::tracker_info::~tracker_info() = default;

  void geoinfo::tracker_info::add_station(station_ptr&& p) {
    m_stations.emplace_back(std::move(p));
  }

  void geoinfo::tracker_info::add_volume(const geo_path& p, const gas_volume& v) {
    m_volumes[p] = v;
  }

  double geoinfo::tracker_info::minimum_distance(const wire& w1, const wire& w2) {
    dir_3d s = w1.direction();
    dir_3d r = w2.direction();
    dir_3d diff = dir_3d(w1.head - w2.head);
    double ss = s.Dot(s);
    double sr = s.Dot(r);
    double rr = r.Dot(r);
    double sd = s.Dot(diff);
    double rd = r.Dot(diff);
    double det = ss * rr - sr * sr;
    if (std::fabs(det) > 1e-9) { // non parallel segments
      double t1 = (sr * rd - rr * sd) / det;
      double t2 = (ss * rd - sr * sd) / det;
      t1 = std::clamp(t1, 0.0, 1.0);
      t2 = std::clamp(t2, 0.0, 1.0);
      pos_3d p1 = w1.head + t1 * w1.direction();
      pos_3d p2 = w2.head + t2 * w2.direction();
      if (t1 == 0 || t1 == 1) {
        dir_3d ap = dir_3d(p1 - w2.head);
        t2 = ap.Dot(r) / rr;
        t2 = std::clamp(t2, 0.0, 1.0);
      }
      if (t2 == 0 || t2 == 1) {
        dir_3d ap = dir_3d(p2 - w1.head);
        t1 = ap.Dot(s) / ss;
        t1 = std::clamp(t1, 0.0, 1.0);
      }
      p1 = w1.head + t1 * w1.direction();
      p2 = w2.head + t2 * w2.direction();
      return std::sqrt((p1 - p2).Mag2());
    } else { // parallel segments
      double t = rd / r.Mag2();
      pos_3d p = w2.head + t * r;
      return std::sqrt((w1.head - p).Mag2());
    }


  }

}
