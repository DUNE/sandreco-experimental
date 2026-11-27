#pragma once

#include <common/sand.h>

#include <ufw/config.hpp>

#include <memory>

namespace sand {

  /**
   * Implementation guide: all members that belong to a specific subdetector go in its _info class.
   * Keep only general purpose members in the main class, and keep the value of any configuration parameter.
   */
  class geoinfo : ufw::data::base<ufw::data::complex_tag, ufw::data::unique_tag, ufw::data::global_tag> {
   public:
    class subdetector_info;
    class ecal_info;
    class grain_info;
    class tracker_info;
    class drift_info;
    class generic_drift_info;
    class stt_info;
    class position_info;
    class field_info;
    class uniform_field;
    class volume_field;
    class field_probe;

    /// Thrown by field(), field_at() and make_field_probe() when the configuration has no "field" key.
    class field_not_configured : public ufw::exception {
     public:
      field_not_configured()
        : ufw::exception("sand::geoinfo: no magnetic field configured. Add a \"field\" key to the sand::geoinfo "
                         "configuration: [Bx, By, Bz] in T, or {{ \"source\" : \"gdml\", \"file\" : <path> }}.") {}
    };

   private:
    class invalid_geo_id : public ufw::exception {
     public:
      invalid_geo_id(geo_id g) : ufw::exception("GUID {:x} is not a valid identifier.", g.raw) {}
    };

   public:
    explicit geoinfo(const ufw::config&);

    position_info get_info(const pos_3d&);

    ~geoinfo();

    const ecal_info& ecal() const {
      if (!m_ecal) {
        init_ecal();
      }
      return *m_ecal;
    }

    const grain_info& grain() const {
      if (!m_grain) {
        init_grain();
      }
      return *m_grain;
    }

    const tracker_info& tracker() const {
      if (!m_tracker) {
        init_tracker();
      }
      return *m_tracker;
    }

    /**
     * @name Magnetic field
     *
     * The field is configured by the "field" key of the geoinfo configuration (see field_info::create() and
     * docs/geoinfo.md): a uniform field `[Bx, By, Bz]` in T, or `{ "source" : "gdml", "file" : <path> }` for the
     * field of the geometry volumes read from the BField tags of the GDML. Without the key there is no field and
     * field_at() and make_field_probe() throw field_not_configured; nothing else changes.
     *
     * Positions are in mm in the global frame of the geometry loaded by sand::root_tgeomanager; fields are in tesla.
     * The declarations of field_info and field_probe are in geoinfo/field_info.hpp.
     *
     * \par Threads
     * geoinfo is shared by all modules and keeps no lookup state for the field. field_at() uses the shared navigator
     * of sand::root_tgeomanager, as get_info() does, so it must not be called from several threads at once and it
     * moves that navigator. A field_probe has its own navigator and cache: keep one per caller (and per thread).
     * @{
     */

    /// Whether a field is configured
    bool has_field() const { return static_cast<bool>(m_field); }

    /**
     * @brief The configured field.
     * @throws field_not_configured if there is no "field" key
     */
    const field_info& field() const;

    /**
     * @brief Magnetic field [T] at a global position [mm].
     *
     * Each call locates the point from the top of the geometry with the shared navigator, which is moved; for
     * lookups at nearby points (propagation) a field_probe is up to about twice as fast. Points outside the world
     * have zero field.
     *
     * @throws field_not_configured if there is no "field" key
     */
    dir_3d field_at(const pos_3d& p) const;

    /**
     * @brief A new probe for fast field lookups, to be kept by the caller (see field_probe).
     * @throws field_not_configured if there is no "field" key
     */
    field_probe make_field_probe() const;
    /// @}

    geo_id id(const geo_path&) const;

    geo_path path(geo_id) const;

   private:
    void init_ecal() const;
    void init_grain() const;
    void init_tracker() const;

   private:
    friend class subdetector_info;
    friend class ecal_info;
    friend class grain_info;
    friend class tracker_info;
    friend class drift_info;
    friend class generic_drift_info;
    friend class stt_info;
    friend class position_info;

   private:
    mutable std::shared_ptr<grain_info> m_grain;
    mutable std::shared_ptr<ecal_info> m_ecal;
    mutable std::shared_ptr<tracker_info> m_tracker;
    geo_path m_root_path;
    ufw::config m_grain_cfg;
    ufw::config m_ecal_cfg;
    ufw::config m_tracker_cfg;
    subdetector_t m_tracker_type = NONE;
    std::unique_ptr<const field_info> m_field; ///< Magnetic field, null if not configured

    const geo_path& root_path() const { return m_root_path; }

   public:
    static bool getXYLineSegmentIntersection(const pos_3d& v1, const pos_3d& v2, const pos_3d& p, const dir_3d& dir,
                                             pos_3d& intersection_point);

    static std::vector<pos_3d> getXYLinePolygonIntersections(const std::vector<pos_3d>& polygon,
                                                             const pos_3d& line_point, const dir_3d& line_dir);
  };

} // namespace sand

UFW_DECLARE_COMPLEX_DATA(sand::geoinfo);
