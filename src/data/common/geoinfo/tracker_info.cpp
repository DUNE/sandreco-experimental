/**
 * @file
 * @brief Implementation of the common tracker geometry (@ref sand::geoinfo::tracker_info)
 *        together with its wire, station and bounding-box helpers.
 */

#include <BVH.hpp>
#include <tracker_info.hpp>
#include <tracker_material.hpp>

#include <TGeoBBox.h>
#include <TGeoMaterial.h>
#include <TGeoMatrix.h>
#include <TGeoNavigator.h>
#include <TGeoNode.h>
#include <TGeoVolume.h>

#include <cmath>
#include <map>

namespace sand {

  /**
   * @brief Corrects a measured point for wire sag (catenary) along this wire.
   *
   * Applies the catenary of the relevant wire segment as a vertical compensation,
   * so the returned point sits slightly higher than the input.
   *
   * @param measured Point referenced to this wire, in global coordinates.
   * @return The sag-corrected position in global coordinates.
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
   * @brief Index of the wire segment (between spacers) closest to a point.
   * @param x A point along the wire.
   * @return The segment index, guaranteed to be <= s_max_wire_spacers.
   */
  std::size_t geoinfo::tracker_info::wire::segment(pos_3d x) const {
    double t      = x.x() - std::min(head.x(), tail.x());
    std::size_t i = 0;
    while (i != s_max_wire_spacers && spacers[i] > t) {
      ++i;
    }
    return i;
  }

  /**
   * @brief Constructs the common tracker geometry from its geometry sub-tree.
   * @param gi The parent geometry description.
   * @param gp The geometry path of the tracker volume.
   */
  geoinfo::tracker_info::tracker_info(const geoinfo& gi, const geo_path& gp, const ufw::config&)
    : subdetector_info(gi, gp) {}

  /// @brief Default destructor.
  geoinfo::tracker_info::~tracker_info() = default;

  /**
   * @brief Appends a station to the tracker, taking ownership of it.
   * @param p The station to move in.
   */
  void geoinfo::tracker_info::add_station(station_ptr&& p) { m_stations.emplace_back(std::move(p)); }

  /**
   * @brief Sum of x/X0 over the material layers of the station.
   * @return The thickness in radiation lengths along the sampling line (dimensionless).
   */
  double geoinfo::tracker_info::station::radiation_lengths() const {
    double sum = 0.0;
    for (const auto& l : layers) {
      sum += l.x_over_x0();
    }
    return sum;
  }

  /**
   * @brief Describes the material of a station by walking the geometry along z through one (x, y) point.
   *
   * The navigator is placed at (x, y), by default the centre of the station box, on the upstream face of the box,
   * and stepped along +z
   * with FindNextBoundaryAndStep up to the downstream face. Each step inside a volume that belongs to the station
   * (its path starts with the station path) gives a layer. Consecutive steps in the same volume are merged; steps
   * in different volumes stay separate layers even if the material is the same, so that e.g. a target and an
   * adjacent foil of the same plastic remain distinct.
   * Steps in volumes of other stations (overlaps at the faces) and steps shorter than 1 pm are dropped.
   *
   * The result describes the material along that line only. In the STT this is one particular sequence of straw
   * walls and gas, not an average over the straw plane; the STT passes a point halfway between two adjacent wires
   * so that no wire is crossed head-on.
   *
   * The navigator state is changed: callers that iterate over nodes must cd again afterwards.
   *
   * @param nav The navigator to use.
   * @param station_path Geometry path of the station; its shape must be a box.
   * @param xy Global (x, y) of the sampling line [mm]; the centre of the station box if empty.
   * @return The layers, ordered by z and not overlapping.
   */
  std::vector<geoinfo::tracker_info::material_layer>
  geoinfo::tracker_info::sample_material_layers(TGeoNavigator& nav, const geo_path& station_path,
                                                std::optional<std::pair<double, double>> xy) {
    constexpr double min_thickness  = 1e-9; // mm
    constexpr std::size_t max_steps = 100000;

    if (!nav.cd(station_path.c_str())) {
      UFW_ERROR("Cannot find station '{}' to sample its material.", station_path.c_str());
    }
    const std::string prefix = nav.GetPath();
    auto* box                = dynamic_cast<const TGeoBBox*>(nav.GetCurrentVolume()->GetShape());
    if (!box) {
      UFW_ERROR("Station '{}' is not a box, cannot sample its material.", prefix);
    }
    const double origin[3] = {box->GetOrigin()[0], box->GetOrigin()[1], box->GetOrigin()[2]};
    double centre[3];
    nav.LocalToMaster(origin, centre);
    // Half extent of the (possibly rotated) box along global z.
    const double* rot = nav.GetCurrentMatrix()->GetRotationMatrix();
    const double half_z =
        std::abs(rot[6]) * box->GetDX() + std::abs(rot[7]) * box->GetDY() + std::abs(rot[8]) * box->GetDZ();
    if (xy) {
      centre[0] = xy->first;
      centre[1] = xy->second;
    }
    const double z_begin = centre[2] - half_z;
    const double z_end   = centre[2] + half_z;

    std::map<const TGeoMaterial*, material_layer> cache;
    std::vector<material_layer> layers;
    const TGeoVolume* last_volume = nullptr;
    nav.InitTrack(centre[0], centre[1], z_begin, 0.0, 0.0, 1.0);
    double z = z_begin;
    for (std::size_t step = 0; z < z_end - min_thickness; ++step) {
      if (step == max_steps || nav.IsOutside()) {
        UFW_ERROR("Lost the navigator while sampling the material of station '{}' at z = {} mm.", prefix, z);
      }
      const TGeoNode* node   = nav.GetCurrentNode();
      const std::string path = nav.GetPath();
      const bool inside =
          path.compare(0, prefix.size(), prefix) == 0 && (path.size() == prefix.size() || path[prefix.size()] == '/');
      nav.FindNextBoundaryAndStep(z_end - z);
      const double z_next = std::min(nav.GetCurrentPoint()[2], z_end);
      if (!(z_next > z)) {
        // No progress (e.g. a zero-length step on a boundary): push the point forward and locate it again.
        nav.SetCurrentPoint(centre[0], centre[1], z + min_thickness);
        nav.FindNode();
        z += min_thickness;
        continue;
      }
      if (inside && z_next - z > min_thickness) {
        const TGeoVolume* vol   = node->GetVolume();
        const TGeoMaterial* mat = vol->GetMaterial();
        if (!layers.empty() && vol == last_volume && std::abs(layers.back().z_max - z) < min_thickness) {
          layers.back().z_max = z_next;
        } else {
          auto it = cache.find(mat);
          if (it == cache.end()) {
            it = cache.emplace(mat, tracker_material::properties(*mat)).first;
          }
          material_layer l = it->second;
          l.z_min          = z;
          l.z_max          = z_next;
          layers.push_back(std::move(l));
        }
        last_volume = vol;
      }
      z = z_next;
    }
    return layers;
  }

  /**
   * @brief Sets the target box and density of a station from its target volume.
   * @param stat The station to update.
   * @param nav A navigator whose current node is the parent of @p target (its rotation is used).
   * @param target The target node, its shape must be a box.
   * @throws ufw::exception if the shape is not a box or the density is not plausible (wrong units).
   */
  void geoinfo::tracker_info::set_target(station& stat, TGeoNavigator& nav, const TGeoNode& target) {
    auto* shape = dynamic_cast<const TGeoBBox*>(target.GetVolume()->GetShape());
    if (!shape) {
      UFW_ERROR("Target shape is not a Box");
    }
    const double half[3] = {shape->GetDX(), shape->GetDY(), shape->GetDZ()};
    double master[3];
    nav.LocalToMasterVect(half, master);
    // The rotation can flip the sign of the components, the size is positive.
    stat.target_box     = 2.0 * dir_3d(std::abs(master[0]), std::abs(master[1]), std::abs(master[2]));
    stat.target_density = tracker_material::density(*target.GetVolume()->GetMaterial());
    if (!(stat.target_density > 0.1 && stat.target_density < 30.0)) {
      UFW_ERROR("Target '{}' has density {} g/cm3: the geometry densities are not in Geant4 units?", target.GetName(),
                stat.target_density);
    }
  }

  /**
   * @brief Registers a gas volume at a geometry path.
   * @param p The geometry path of the volume.
   * @param v The gas volume.
   */
  void geoinfo::tracker_info::add_volume(const geo_path& p, const gas_volume& v) { m_volumes[p] = v; }

  /**
   * @brief Closest-approach parameters between a segment and this wire.
   *
   * Solves for the mutually closest points of the segment and the finite wire,
   * falling back to the segment midpoint when the two are parallel.
   *
   * @param seg_start Start of the segment.
   * @param seg_stop  End of the segment.
   * @return {t along the segment in [0,1], t' along the wire in [0,1]}.
   */
  std::pair<double, double> geoinfo::tracker_info::wire::closest_approach_segment(
      const pos_3d& seg_start, // TO-DO should always produce a result, even in the parallel case
      const pos_3d& seg_stop) const {
    std::pair<double, double> result;

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
      double t       = 0.5;
      double t_prime = closest_approach_point(seg_start + s * t);

      result.first  = t;
      result.second = t_prime;

      return result;
    } else {
      double t       = (B * E - C * D) / denominator; // Parameter along s
      double t_prime = (A * E - B * D) / denominator; // Parameter along r

      // Clamp t to [0, 1] to stay within the segment
      t                    = std::max(0.0, std::min(1.0, t));
      pos_3d clamped_point = seg_start + s * t;
      t_prime              = closest_approach_point(clamped_point);

      result.first  = t;
      result.second = t_prime;

      return result;
    }
  }

  /**
   * @brief Distance of closest approach between a segment and this wire.
   * @param seg_start Start of the segment.
   * @param seg_stop  End of the segment.
   * @return The minimum distance between the segment and the wire.
   */
  double geoinfo::tracker_info::wire::closest_approach_segment_distance(const pos_3d& seg_start,
                                                                        const pos_3d& seg_stop) const {
    auto approach                   = closest_approach_segment(seg_start, seg_stop);
    pos_3d closest_point_on_segment = seg_start + (seg_stop - seg_start) * approach.first;
    pos_3d closest_point_on_wire    = head + direction() * approach.second;
    return sqrt((closest_point_on_segment - closest_point_on_wire).Mag2());
  }

  /**
   * @brief Parameter of the point on this wire closest to a given point.
   * @param point A point in global coordinates.
   * @return The parameter t' along the wire, clamped to [0,1].
   */
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

  /**
   * @brief Transform from the wire-plane local frame to global coordinates.
   * @return The transform built from the wire angle and the station centre.
   */
  xform_3d geoinfo::tracker_info::wire::wire_plane_transform() const {
    double c      = std::cos(angle());
    double s      = std::sin(angle());
    pos_3d center = pos_3d(0.5 * (parent->top_north.X() + parent->bottom_south.X()),
                           0.5 * (parent->top_north.Y() + parent->bottom_south.Y()), head.Z());
    xform_3d wire_rot(c, -s, 0., center.X(), s, c, 0., center.Y(), 0., 0., 1, center.Z());
    return wire_rot;
  }

  /**
   * @brief Finds the wire of a list closest to a point.
   * @param list  The candidate wires.
   * @param point The reference point.
   * @return {pointer to the closest wire (nullptr if the list is empty), its index in the list}.
   */
  std::pair<const geoinfo::tracker_info::wire*, size_t>
  geoinfo::tracker_info::closest_wire_in_list(wire_list list, pos_3d point) const {
    double min_dist                                 = std::numeric_limits<double>::max();
    const geoinfo::tracker_info::wire* closest_wire = nullptr;
    size_t closest_wire_index                       = 0;
    size_t current_index                            = 0;

    for (const auto wire : list) {
      auto closest_point_param = wire->closest_approach_point(point);

      double dist = sqrt((pos_3d(wire->head + wire->direction() * closest_point_param) - point).Mag2());

      if (dist < min_dist) {
        min_dist           = dist;
        closest_wire       = wire;
        closest_wire_index = current_index;
      }
      ++current_index;
    }

    return {closest_wire, closest_wire_index};
  }

  /**
   * @brief Returns the wire connected to a given DAQ channel.
   * @param chid The DAQ channel identifier.
   * @return The matching wire; raises UFW_ERROR if no wire matches.
   */
  const geoinfo::tracker_info::wire& geoinfo::tracker_info::wire_at(channel_id chid) const {
    const wire* retw = nullptr;
    for_each_station([chid, &retw](const station& stat) {
      if (stat.daq_link != chid.link) {
        return;
      }
      for (const auto& w : stat.wires) {
        if (w->daq_channel == chid) {
          retw = w.get();
          break;
        }
      }
    });
    if (retw) {
      return *retw;
    } else {
      UFW_ERROR("There is no wire corresponding to channel {}.", chid);
    }
  }

  /**
   * @brief Total target mass per material, summed over all stations.
   * @return A map from target material to its mass [kg].
   */
  geoinfo::tracker_info::target_mass_t geoinfo::tracker_info::target_masses() const {
    constexpr double kg_per_g_cm3_mm3 = 1e-6; // 1 g/cm3 * 1 mm3 = 1e-3 g
    target_mass_t map;
    for_each_station([&map](const station& stat) {
      double vol = stat.target_box.x() * stat.target_box.y() * stat.target_box.z(); // mm3
      map[stat.target] += vol * stat.target_density * kg_per_g_cm3_mm3;
    });
    return map;
  }

  /**
   * @brief Whether a wire is among this wire's precomputed neighbours.
   * @param w The candidate neighbour.
   * @return true if @p w is adjacent to this wire.
   */
  bool geoinfo::tracker_info::wire::is_adjacent(const geoinfo::tracker_info::wire* w) const {
    for (const auto* adj : adjacent_wires) {
      if (adj == w) {
        return true;
      }
    }
    return false;
  }

  /**
   * @brief Builds the wire adjacency of the station using a bounding-volume hierarchy.
   */
  void geoinfo::tracker_info::station::set_wire_adjacency() {
    double dz;
    double dy;
    dy = wires[0]->max_radius * sqrt(3) / 2.;
    dz = wires[0]->max_radius * sqrt(3) / 2.;

    double max_distance = sqrt(dy * dy + dz * dz) + 0.1;

    wire_list wl;
    wl.reserve(wires.size());
    for (const auto& wp : wires)
      wl.push_back(wp.get());

    BVH bvh(wl, 2 * max_distance, 2 * max_distance);

    bvh.printTreeInfo();
  }

  /**
   * @brief Builds the axis-aligned bounding box that encloses a wire.
   * @param w The wire to bound.
   */
  geoinfo::tracker_info::wire::AABB::AABB(const geoinfo::tracker_info::wire& w) {
    min_ = pos_3d(1E9, 1E9, 1E9);
    max_ = pos_3d(-1E9, -1E9, -1E9);
    std::array<pos_3d, 4> vertices;
    auto p                = w.head;
    const auto& transform = w.wire_plane_transform();
    auto p_rotated        = transform.Inverse() * p;

    // auto h_2 = wire->length() /2.;
    auto w_2 = w.max_radius / 2.;

    pos_3d v1 = p_rotated + dir_3d(0, w_2, 0);
    pos_3d v2 = p_rotated + dir_3d(0, -w_2, 0);

    auto p1 = transform * v1;
    auto p2 = transform * v2;

    vertices[0] = pos_3d(p1.X(), p1.Y(), p.Z() + w_2);
    vertices[1] = pos_3d(p2.X(), p2.Y(), p.Z() + w_2);

    p         = w.tail;
    p_rotated = transform.Inverse() * p;
    pos_3d v3 = p_rotated + dir_3d(0, w_2, 0);
    pos_3d v4 = p_rotated + dir_3d(0, -w_2, 0);

    auto p3 = transform * v3;
    auto p4 = transform * v4;

    vertices[2] = pos_3d(p3.X(), p3.Y(), p.Z() - w_2);
    vertices[3] = pos_3d(p4.X(), p4.Y(), p.Z() - w_2);

    for (const auto& v : vertices) {
      min_.SetX(std::min(min_.X(), v.X()));
      max_.SetX(std::max(max_.X(), v.X()));
      min_.SetY(std::min(min_.Y(), v.Y()));
      max_.SetY(std::max(max_.Y(), v.Y()));
      min_.SetZ(std::min(min_.Z(), v.Z()));
      max_.SetZ(std::max(max_.Z(), v.Z()));
      // UFW_DEBUG("Vertex: ({}, {}, {})", v.X(), v.Y(), v.Z());
    }
  }

  /**
   * @brief Grows this box so that it also contains another box.
   * @param second_aabb The box to merge in.
   */
  void geoinfo::tracker_info::wire::AABB::expand(const AABB& second_aabb) {
    min_.SetX(std::min(min_.X(), second_aabb.min_.X()));
    max_.SetX(std::max(max_.X(), second_aabb.max_.X()));
    min_.SetY(std::min(min_.Y(), second_aabb.min_.Y()));
    max_.SetY(std::max(max_.Y(), second_aabb.max_.Y()));
    min_.SetZ(std::min(min_.Z(), second_aabb.min_.Z()));
    max_.SetZ(std::max(max_.Z(), second_aabb.max_.Z()));
  }

  /**
   * @brief Tests whether two boxes overlap within a tolerance.
   * @param second_aabb The other box.
   * @param epsilon     Tolerance added to the overlap test.
   * @return true if the boxes overlap.
   */
  bool geoinfo::tracker_info::wire::AABB::isOverlapping(const AABB& second_aabb, double epsilon) const {
    return (max_.X() + epsilon >= second_aabb.min_.X() && min_.X() - epsilon <= second_aabb.max_.X()
            && max_.Y() + epsilon >= second_aabb.min_.Y() && min_.Y() - epsilon <= second_aabb.max_.Y()
            && max_.Z() + epsilon >= second_aabb.min_.Z() && min_.Z() - epsilon <= second_aabb.max_.Z());
  }

} // namespace sand
