#pragma once

#include <kalman/field.hpp>

#include <root_tgeomanager/root_tgeomanager.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class TGeoVolume;

/**
 * @file geometry_field.hpp
 * @brief Piecewise constant magnetic field read from the BField auxiliary tags of the GDML.
 */

namespace sand::tracker::kalman {

  /**
   * @brief Field tag of one GDML volume.
   *
   * A volume with a `BField` auxiliary has that field. A volume with only an `EField` auxiliary has a zero
   * magnetic field: edep-sim gives such a volume a field manager of its own, with B = 0, which hides the field of its
   * ancestors.
   */
  struct gdml_field_tag {
    std::string volume; ///< Name of the GDML logical volume
    field_vector b{};   ///< Magnetic field [T]; zero for a volume tagged with EField only
  };

  /**
   * @brief Parse the value of a GDML BField auxiliary, e.g. "(0.6 T, 0.0 T, 0.0 T)".
   *
   * The format is the one read by edep-sim: three comma-separated components, each a number optionally followed by a
   * unit of the Geant4 units table (T, tesla, kG, kilogauss, G, gauss); a missing unit means tesla. The surrounding
   * parentheses are optional.
   *
   * @return The field [T]
   * @throws ufw::exception on a malformed value or an unknown unit
   */
  field_vector parse_bfield(std::string_view value);

  /**
   * @brief Read the field tags of the volumes of a GDML file.
   *
   * Only the `<volume>` elements and their `BField` and `EField` `<auxiliary>` children are read, with ROOT's XML
   * parser: no geometry is built. External XML entities are not expanded, so tags in files included through entities
   * are not seen.
   *
   * @param gdml Path of the GDML file
   * @return One entry per volume with a BField or an EField tag, in file order
   * @throws ufw::exception if the file cannot be parsed or a BField value is malformed
   */
  std::vector<gdml_field_tag> read_gdml_field_tags(const std::filesystem::path& gdml);

  /// How one field tag is applied to the geometry: summary of the field table of volume_field.
  struct field_tag_usage {
    std::string volume;             ///< Name of the GDML volume of the tag
    std::string geometry_volume;    ///< Name of the matching volume of the loaded geometry (V or V_PV)
    field_vector b{};               ///< Field of the tag [T]
    bool placed            = false; ///< Whether the volume is placed in the geometry tree (reached from the top volume)
    std::size_t inheriting = 0;     ///< Number of other logical volumes that take their field from this tag
  };

  /**
   * @brief Magnetic field of the volume containing the point, from GDML field tags applied to the loaded geometry.
   *
   * The geometry is the TGeoManager of sand::root_tgeomanager, usually loaded from an edep-sim file. That geometry
   * does not keep the GDML auxiliaries, so the tags are read from the GDML (read_gdml_field_tags()) and matched to the
   * loaded volumes by name: a GDML volume `V` is the TGeoVolume `V` or, as written by edep-sim, `V_PV`.
   *
   * The field is piecewise constant. It follows the rule of edep-sim, which attaches a field manager to each tagged
   * logical volume and forces it onto all its daughters, visiting the tree from the top: a volume has the field of its
   * nearest tagged ancestor, itself included. A volume without a tagged ancestor, and any point outside the world,
   * has zero field. The table is computed once per logical volume at construction. As in edep-sim, the field belongs
   * to the logical volume and is shared by all its placements: a volume placed both under a tagged ancestor and
   * elsewhere has the tagged field everywhere. A volume that inherits two different fields through different
   * placements is an error, since in edep-sim its field would depend on the order in which the volumes are visited.
   * In the SAND geometry every logical volume has a single mother volume, so neither case occurs.
   *
   * At construction a summary of the table is logged at info level: each tag with its volume, its field and the number
   * of volumes inheriting it, and the number of volumes with no field (no tagged ancestor). The same information is
   * available from tag_usage() and untagged_volumes().
   *
   * \par Performance
   * A lookup calls TGeoNavigator::IsSameLocation(x, y, z, true), which checks whether the point is still in the last
   * volume and otherwise relocates the navigator, searching from the current location. The field of the last volume
   * is cached. On the SAND geometry this costs about 0.1 us per call for points 0.5 mm apart and 0.25 us for points
   * 25 mm apart.
   *
   * \par Threads
   * at() is const but moves the navigator and updates the cache (mutable members). One instance, and so one
   * sand::tracker::kalman::engine, must not be used from several threads at once.
   *
   * \par Navigator
   * The navigator is the one returned by sand::root_tgeomanager::navigator(), shared with other users of the
   * geometry. at() reads the volume from the navigator state at each call, so it is correct whatever other code did
   * with the navigator in between; conversely, a caller must not rely on the navigator position across a call to
   * at().
   *
   * \par Discontinuities
   * The propagator Jacobian assumes no field gradient, and a Runge-Kutta step crossing a field boundary (magnet
   * volume to yoke) is less accurate than a step in a uniform field. Inside the volume containing the tracker the
   * field is uniform, so tracker fits are not affected; propagation through the calorimeter and the yoke is.
   */
  class volume_field : public field {
   public:
    /**
     * @param geo  Geometry the field is applied to
     * @param tags Field tags, typically from read_gdml_field_tags()
     * @throws ufw::exception if a tagged volume is not in the geometry, or a volume inherits two different fields
     */
    volume_field(sand::root_tgeomanager& geo, const std::vector<gdml_field_tag>& tags);
    ~volume_field() override;

    /// Field [T] at the global position (x, y, z) [mm]
    field_vector at(double x, double y, double z) const override;

    /// Field [T] of a logical volume of the geometry, inheritance included; zero for an unknown volume
    field_vector volume_value(const TGeoVolume* v) const;

    /// Number of logical volumes with a non-zero field
    std::size_t magnetised_volumes() const;

    /// For each field tag, in the order given to the constructor: matching volume, field and inheriting volumes
    const std::vector<field_tag_usage>& tag_usage() const { return m_usage; }

    /// Number of logical volumes in the geometry tree without a tagged ancestor, hence with zero field
    std::size_t untagged_volumes() const { return m_untagged; }

   private:
    sand::root_tgeomanager::navigator_ptr m_nav;                 ///< Navigator used by at()
    std::unordered_map<const TGeoVolume*, field_vector> m_table; ///< Field of each logical volume reached
    mutable const TGeoVolume* m_last_volume = nullptr;           ///< Volume found by the last call of at()
    mutable field_vector m_last_b{};                             ///< Field of m_last_volume
    std::vector<field_tag_usage> m_usage;                        ///< Summary of the table, per tag
    std::size_t m_untagged = 0;                                  ///< Volumes without a tagged ancestor
  };

} // namespace sand::tracker::kalman
