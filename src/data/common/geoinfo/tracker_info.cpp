#include <tracker_info.hpp>

namespace sand {

  /**
   * Calculates the y-offset correction due to wire sag from a @p measured point what is referenced to this wire.
   * The correction is applied as a compensation i.e. the return value is vertically higher than the input.
   *
   * @returns The corrected position of the point in global coordinates
   */
  pos_3d geoinfo::tracker_info::wire::actual(pos_3d measured) const {
    auto seg     = segment(measured);
    catenary cat = catenaries[seg];
    auto real    = measured;
    auto yoff    = cat.a * std::cosh((measured.x() - cat.minimum.x()) / cat.a) - cat.a;
    real.SetY(measured.y() + yoff);
    return real;
  }

  /**
   * @returns the index of the segment of wire that this point is closest to, guaranteed to be <= s_max_wire_spacers
   */
  std::size_t geoinfo::tracker_info::wire::segment(pos_3d x) const {
    double t      = x.x() - std::min(head.x(), tail.x());
    std::size_t i = 0;
    while (i != s_max_wire_spacers && spacers[i] > t) {
      ++i;
    }
    return i;
  }

  geoinfo::tracker_info::tracker_info(const geoinfo& gi, const geo_path& p) : subdetector_info(gi, p) {}

  geoinfo::tracker_info::~tracker_info() = default;

  void geoinfo::tracker_info::add_station(station_ptr&& p) { m_stations.emplace_back(std::move(p)); }

  void geoinfo::tracker_info::add_volume(const geo_path& p, const gas_volume& v) { m_volumes[p] = v; }

  std::vector<vec_4d> geoinfo::tracker_info::closest_points(const vec_4d& hit_start, const vec_4d& hit_stop,
                                                            const double& v_drift, const wire& w) const {
    std::vector<vec_4d> closest_points;

    pos_3d start(hit_start.X(), hit_start.Y(), hit_start.Z());
    pos_3d stop(hit_stop.X(), hit_stop.Y(), hit_stop.Z());

    std::vector<double> seg_params = w.closest_approach_segment(start, stop);

    std::vector<vec_4d> result;

    if (seg_params.empty() == false) {
      double& t       = seg_params[0]; // Parameter along s
      double& t_prime = seg_params[1]; // Parameter along r

      // Calculate the closest point on the line segment
      pos_3d closest_point_hit = start + (stop - start) * t;

      if (t == 0 || t == 1) {
        dir_3d AP = closest_point_hit - w.head;
        t_prime   = AP.Dot(w.direction()) / w.direction().Mag2();
        t_prime   = std::max(0.0, std::min(1.0, t_prime));
      }

      // Calculate the corresponding point on the wire
      pos_3d closest_point_wire = w.head + w.direction() * t_prime;

      double fraction = sqrt((closest_point_hit - start).Mag2() / (stop - start).Mag2());
      vec_4d closest_point_hit_l(closest_point_hit.X(), closest_point_hit.Y(), closest_point_hit.Z(),
                                 hit_start.T() + fraction * (hit_stop.T() - hit_start.T()));

      result.push_back(closest_point_hit_l);

      vec_4d closest_point_wire_l(closest_point_wire.X(), closest_point_wire.Y(), closest_point_wire.Z(),
                                  closest_point_hit_l.T()
                                      + sqrt((closest_point_hit - closest_point_wire).Mag2()) / v_drift);

      result.push_back(closest_point_wire_l);

      return result;

    } else {
      // Lines are parallel; handle this case if necessary
      UFW_WARN("Lines are parallel; no unique closest point.");
      return result;
    }
  }

  double geoinfo::tracker_info::get_min_time(const vec_4d& point, const double& v_signal_inwire, const wire& w) const {
    return point.T() + sqrt((pos_3d(point.Vect()) - w.head).Mag2()) / v_signal_inwire;
  }

  std::vector<double> geoinfo::tracker_info::wire::closest_approach_segment(const pos_3d& seg_start,
                                                                            const pos_3d& seg_stop) const {
    std::vector<double> result;

    dir_3d d = seg_start - head;     // Vector from wire head to point start
    dir_3d s = seg_stop - seg_start; // Vector from point start to point stop
    dir_3d r = direction();          // Wire direction vector

    double A = s.Dot(s); // s . s
    double B = s.Dot(r); // s . r
    double C = r.Dot(r); // r . r
    double D = s.Dot(d); // s . (start - head)
    double E = r.Dot(d); // r . (start - head)

    double denominator = A * C - B * B;

    if (std::abs(denominator) < 1e-8) {
      // Lines are parallel; handle this case if necessary
      UFW_WARN("Lines are parallel; no unique closest point.");
      return result;
    } else {
      double t       = (B * E - C * D) / denominator; // Parameter along s
      double t_prime = (A * E - B * D) / denominator; // Parameter along r

      // Clamp t to [0, 1] to stay within the segment
      t       = std::max(0.0, std::min(1.0, t));
      t_prime = std::max(0.0, std::min(1.0, t_prime));

      result.push_back(t);
      result.push_back(t_prime);

      return result;
    }
  }

  double geoinfo::tracker_info::wire::closest_approach_point(const pos_3d& point) const {
    dir_3d d = point - head; // Vector from wire head to the point
    dir_3d r = direction();  // Wire direction vector (should be normalized or not depending on convention)

    double C = r.Dot(r); // r . r
    double E = r.Dot(d); // r . (point - head)

    if (std::abs(C) < 1e-12) {
      // Wire has no direction — degenerate case
      UFW_WARN("Wire direction vector has near-zero length.");
      return std::numeric_limits<double>::quiet_NaN();
    }

    // Parameter along the wire direction from the head to the closest point
    double t_prime = E / C;

    // If the wire has finite length, clamp t' to [0, 1]
    t_prime = std::max(0.0, std::min(1.0, t_prime));

    return t_prime;
  }

} // namespace sand
