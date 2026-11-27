#pragma once

#include <geoinfo/geoinfo.hpp>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class TGeoManager;
class TGeoNavigator;
class TGeoVolume;

/**
 * @file field_info.hpp
 * @brief Magnetic field of the geometry, provided by sand::geoinfo: field descriptions, GDML field tags and the
 *        per-caller lookup object sand::geoinfo::field_probe.
 *
 * Units and frame everywhere in this file: positions in mm in the global frame of the geometry loaded by
 * sand::root_tgeomanager, fields in tesla, as sand::dir_3d vectors.
 */

namespace sand {

  class root_tgeomanager;

  /**
   * @brief Field tag of one GDML volume.
   *
   * A volume with a `BField` auxiliary has that field. A volume with only an `EField` auxiliary has a zero magnetic
   * field: edep-sim gives such a volume a field manager of its own, with B = 0, which hides the field of its ancestors.
   */
  struct gdml_field_tag {
    std::string volume; ///< Name of the GDML logical volume
    dir_3d b;           ///< Magnetic field [T]; zero for a volume tagged with EField only
  };

  /**
   * @brief Parse the value of a GDML BField auxiliary, e.g. "(0.6 T, 0.0 T, 0.0 T)".
   *
   * The format is the one read by edep-sim: three comma-separated components, each a number optionally followed by a
   * unit of the Geant4 units table (T, tesla, kG, kilogauss, G, gauss); a missing unit means tesla. The surrounding
   * parentheses are optional.
   *
   * @param value Text of the auxvalue attribute
   * @return The field [T]
   * @throws ufw::exception on a malformed value or an unknown unit
   */
  dir_3d parse_bfield(std::string_view value);

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

  /// How one field tag is applied to the geometry: summary of the field table of sand::geoinfo::volume_field.
  struct field_tag_usage {
    std::string volume;             ///< Name of the GDML volume of the tag
    std::string geometry_volume;    ///< Name of the matching volume of the loaded geometry (V or V_PV)
    dir_3d b;                       ///< Field of the tag [T]
    bool placed            = false; ///< Whether the volume is placed in the geometry tree (reached from the top volume)
    std::size_t inheriting = 0;     ///< Number of other logical volumes that take their field from this tag
  };

  /**
   * @brief Description of the magnetic field held by sand::geoinfo.
   *
   * Two implementations: sand::geoinfo::uniform_field and sand::geoinfo::volume_field. They are built by
   * sand::geoinfo from the "field" key of its configuration (see create()) and are immutable afterwards.
   */
  class geoinfo::field_info {
   public:
    virtual ~field_info();

    field_info(const field_info&)             = delete;
    field_info& operator= (const field_info&) = delete;

    /**
     * @brief Field [T] at a global position [mm], without a per-caller cache.
     *
     * For sand::geoinfo::volume_field this locates the point from the top of the geometry with the shared navigator
     * of sand::root_tgeomanager, which it moves. Prefer a sand::geoinfo::field_probe for repeated lookups.
     */
    virtual dir_3d at(const pos_3d& p) const = 0;

    /// One-line description of the field, as logged by sand::geoinfo
    virtual std::string description() const = 0;

    /**
     * @brief Build the field described by the value of the "field" key of the sand::geoinfo configuration.
     *
     * Accepted forms:
     * - `[Bx, By, Bz]`: uniform field, components in tesla;
     * - `{ "source" : "gdml", "file" : <path> }`: field of the geometry volumes, from the BField tags of the GDML
     *   file; a relative path is resolved against ufw-basepath (ufw::config::path_at()).
     *
     * Unknown keys and malformed values are rejected with an error naming them, before any file is read.
     *
     * @param cfg Value of the "field" key
     * @param geo Geometry the GDML tags are applied to (unused for a uniform field)
     * @throws ufw::exception on an invalid configuration, an unreadable file or a tag that does not match the geometry
     */
    static std::unique_ptr<const field_info> create(const ufw::json& cfg, root_tgeomanager& geo);

   protected:
    field_info() = default;
  };

  /// The same field everywhere.
  class geoinfo::uniform_field : public field_info {
   public:
    /// @param b The field [T]
    explicit uniform_field(const dir_3d& b) : m_b(b) {}

    /// The field [T], whatever the position
    dir_3d at(const pos_3d&) const override { return m_b; }

    std::string description() const override;

    /// The field [T]
    const dir_3d& value() const { return m_b; }

   private:
    dir_3d m_b; ///< The field [T]
  };

  /**
   * @brief Magnetic field of the volume containing the point, from GDML field tags applied to the loaded geometry.
   *
   * The geometry is the TGeoManager of sand::root_tgeomanager, usually loaded from an edep-sim file, with lengths in
   * mm. That geometry does not keep the GDML auxiliaries, so the tags are read from the GDML (read_gdml_field_tags())
   * and matched to the loaded volumes by name: a GDML volume `V` is the TGeoVolume `V` or, as written by edep-sim,
   * `V_PV`. A tag whose volume is in neither form is an error.
   *
   * The field is piecewise constant. It follows the rule of edep-sim, which attaches a field manager to each tagged
   * logical volume and forces it onto all its daughters, visiting the tree from the top: a volume has the field of its
   * nearest tagged ancestor, itself included. A volume without a tagged ancestor, and any point outside the world,
   * has zero field. As in edep-sim, the field belongs to the logical volume and is shared by all its placements: a
   * volume placed both under a tagged ancestor and elsewhere has the tagged field everywhere. A volume that inherits
   * two different fields through different placements is an error, since in edep-sim its field would depend on the
   * order in which the volumes are visited. In the SAND geometry every logical volume has a single mother volume, so
   * neither case occurs.
   *
   * The table of the field of each logical volume is computed once, at construction, and is read-only afterwards. A
   * summary is logged at info level: each tag with its volume, its field and the number of volumes inheriting it,
   * and the number of volumes with no field (no tagged ancestor). The same information is available from tag_usage()
   * and untagged_volumes().
   */
  class geoinfo::volume_field : public field_info {
   public:
    /**
     * @param geo  Geometry the field is applied to
     * @param tags Field tags, typically from read_gdml_field_tags()
     * @param source Where the tags come from, for description() (e.g. the GDML path)
     * @throws ufw::exception if no geometry is loaded, a tagged volume is not in the geometry, or a volume inherits
     *         two different fields
     */
    volume_field(root_tgeomanager& geo, const std::vector<gdml_field_tag>& tags, std::string source = {});
    ~volume_field() override;

    /**
     * @brief Field [T] at a global position [mm], located from the top of the geometry.
     *
     * Uses the shared navigator of sand::root_tgeomanager (TGeoNavigator::FindNode), which is left at the point.
     */
    dir_3d at(const pos_3d& p) const override;

    std::string description() const override;

    /// Field [T] of a logical volume of the geometry, inheritance included; zero for an unknown volume or nullptr
    dir_3d volume_value(const TGeoVolume* v) const;

    /// Number of logical volumes with a non-zero field
    std::size_t magnetised_volumes() const;

    /// For each field tag, in the order given to the constructor: matching volume, field and inheriting volumes
    const std::vector<field_tag_usage>& tag_usage() const { return m_usage; }

    /// Number of logical volumes in the geometry tree without a tagged ancestor, hence with zero field
    std::size_t untagged_volumes() const { return m_untagged; }

    /// The geometry the table refers to
    TGeoManager* manager() const { return m_manager; }

   private:
    root_tgeomanager* m_geo;                               ///< Owner of the shared navigator used by at()
    TGeoManager* m_manager;                                ///< The loaded geometry
    std::unordered_map<const TGeoVolume*, dir_3d> m_table; ///< Field of each logical volume reached
    std::vector<field_tag_usage> m_usage;                  ///< Summary of the table, per tag
    std::size_t m_untagged = 0;                            ///< Volumes without a tagged ancestor
    std::string m_source;                                  ///< Origin of the tags, for description()
  };

  /**
   * @brief Fast field lookup for one caller, obtained from sand::geoinfo::make_field_probe() and kept by the caller.
   *
   * sand::geoinfo is shared by all modules, so it holds no lookup state. A probe holds its own: for a
   * sand::geoinfo::volume_field, a TGeoNavigator of its own (not the shared navigator of sand::root_tgeomanager) and
   * the field of the last volume found. Each call checks whether the point is still in the last volume
   * (TGeoNavigator::IsSameLocation) and otherwise relocates the navigator, searching from the last location, then
   * reads the volume from the navigator state. The result is therefore always the field of the volume containing the
   * point, and it does not depend on, nor change, the state of the shared navigator: other code may move the shared
   * navigator freely between two calls. For a sand::geoinfo::uniform_field a probe just returns the value.
   *
   * \par Threads
   * field_at() changes the probe (navigator and cache): one probe must not be used from several threads at once.
   * The field table it reads is immutable. Probes used concurrently from different threads are not supported either:
   * ROOT shares the navigation scratch data of the geometry between threads unless the TGeoManager is switched to
   * multi-threaded mode (TGeoManager::SetMaxThreads()), which sandreco does not do.
   *
   * \par Lifetime
   * A probe refers to the field of the sand::geoinfo that made it and to the loaded geometry: it must not outlive
   * them. It is movable, not copyable. A default-constructed or moved-from probe is unbound: field_at() throws.
   */
  class geoinfo::field_probe {
   public:
    /// Unbound probe, to be assigned from sand::geoinfo::make_field_probe()
    field_probe();
    ~field_probe();
    field_probe(field_probe&&) noexcept;
    field_probe& operator= (field_probe&&) noexcept;
    field_probe(const field_probe&)             = delete;
    field_probe& operator= (const field_probe&) = delete;

    /**
     * @brief Field [T] at a global position [mm].
     * @throws ufw::exception if the probe is unbound
     */
    dir_3d field_at(const pos_3d& p);

    /**
     * @brief Probe for field f; for a volume field it creates its own navigator on the geometry of f.
     *
     * Usually obtained through sand::geoinfo::make_field_probe(). The probe keeps a reference to f.
     */
    explicit field_probe(const field_info& f);

    /// Whether the probe is bound to a field
    explicit operator bool() const { return m_field != nullptr; }

   private:
    const field_info* m_field     = nullptr;   ///< The field
    const volume_field* m_volumes = nullptr;   ///< The field, if it is a volume field
    std::unique_ptr<TGeoNavigator> m_nav;      ///< Own navigator (volume field only)
    const TGeoVolume* m_last_volume = nullptr; ///< Volume found by the last call
    dir_3d m_last_b;                           ///< Field of m_last_volume
  };

} // namespace sand
