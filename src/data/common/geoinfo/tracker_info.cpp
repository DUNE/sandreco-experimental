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

  std::pair<double,double> geoinfo::tracker_info::wire::closest_approach_segment(const pos_3d& seg_start, //TO-DO should always produce a result, even in the parallel case
                                                                            const pos_3d& seg_stop) const {
    std::pair<double,double> result;

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
      // Lines are parallel; choose midpoint of the segment
      double t = 0.5;
      double t_prime = closest_approach_point(seg_start + s * t);

      result.first = t;
      result.second = t_prime;

      return result;
    } else {
      double t       = (B * E - C * D) / denominator; // Parameter along s
      double t_prime = (A * E - B * D) / denominator; // Parameter along r

      // Clamp t to [0, 1] to stay within the segment
      t       = std::max(0.0, std::min(1.0, t));
      t_prime = std::max(0.0, std::min(1.0, t_prime));

      result.first = t;
      result.second = t_prime;

      return result;
    }
  }

  double geoinfo::tracker_info::wire::closest_approach_point(const pos_3d& point) const {
    dir_3d d = point - head; // Vector from wire head to the point
    dir_3d r = direction();  // Wire direction vector (should be normalized or not depending on convention)

    double C = r.Dot(r); // r . r
    double E = r.Dot(d); // r . (point - head)

    // Parameter along the wire direction from the head to the closest point
    double t_prime = E / C;

    // If the wire has finite length, clamp t' to [0, 1]
    t_prime = std::max(0.0, std::min(1.0, t_prime));

    return t_prime;
  }

  xform_3d geoinfo::tracker_info::wire::wire_plane_transform() const {
    double c = std::cos(angle());
    double s = std::sin(angle());
    pos_3d center = pos_3d(0.5 * (parent->top_north.X() + parent->bottom_south.X()),
                           0.5 * (parent->top_north.Y() + parent->bottom_south.Y()),
                           head.Z());
    xform_3d wire_rot(c, -s, 0., center.X(),
                      s,  c, 0., center.Y(),
                      0., 0., 1, center.Z());
    return wire_rot;
  }

  std::pair<const geoinfo::tracker_info::wire*, size_t> geoinfo::tracker_info::closest_wire_in_list(wire_list list, pos_3d point) const {
    double min_dist = std::numeric_limits<double>::max();
    const geoinfo::tracker_info::wire* closest_wire = nullptr;
    size_t closest_wire_index = 0;
    size_t current_index = 0;

    for (const auto wire : list) {
      auto closest_point_param = wire->closest_approach_point(point);

      double dist = sqrt((pos_3d(wire->head + wire->direction() * closest_point_param) - point).Mag2());

      if (dist < min_dist) {
        min_dist = dist;
        closest_wire = wire;
        closest_wire_index = current_index;
      }
      ++current_index;
    }

    return {closest_wire, closest_wire_index};
  }

  const geoinfo::tracker_info::wire& geoinfo::tracker_info::wire_at(channel_id chid) const {
    const wire* retw = nullptr;
    for_each_station( [chid, &retw](const station& stat){
      if (stat.daq_link != chid.link) {
        return;
      }
      for (const auto& w: stat.wires) {
        if (w->daq_channel == chid) {
          retw = w.get();
          break;
        }
      }
    } );
    if (retw) {
      return *retw;
    } else {
      UFW_ERROR("There is no wire corresponding to channel {}.", chid);
    }
  }

} // namespace sand
