#pragma once

#include <kalman/field.hpp>
#include <kalman/measurement.hpp>
#include <kalman/propagator.hpp>
#include <kalman/state.hpp>

#include <common/data.h>
#include <ufw/config.hpp>

#include <memory>
#include <optional>

/**
 * @file engine.hpp
 * @brief Complex data object holding the state shared by the tracker reconstruction modules.
 */

namespace sand::tracker::kalman {

  /**
   * \class sand::tracker::kalman::engine
   *
   * \brief Shared state of the tracker Kalman filter: magnetic field, reconstruction calibration and propagation
   * settings.
   *
   * The seeder, the combinatorial track finder, the fit and smoother and the vertex fit all need the same field,
   * calibration and propagation settings. The engine is a global, unique complex data object, so it is built once
   * per job from the "globals" section of the configuration and every module sees the same instance:
   *
   * \code{.cpp}
   * auto& kf = instance<sand::tracker::kalman::engine>();
   * auto prop = kf.propagate(current, w.z(), mass);
   * \endcode
   *
   * The operations that need the shared state are available as members (propagate(), wire_time()). The free
   * functions of the library (sand::tracker::kalman::propagate(), sand::tracker::kalman::wire_time(), ...) remain
   * available with explicit arguments; the members call them.
   *
   * The measurement model applies to drift chambers and straw tubes alike.
   *
   * \subsection Configuration
   * Key `"sand::tracker::kalman::engine"` in the `"globals"` section of the job.
   *
   * | Parameter Name               | Type        | Unit  | Required/Default | Description                                    |
   * |------------------------------|-------------|-------|------------------|------------------------------------------------|
   * | `field`                      | see below   | T     | Required         | Uniform field or field of the volumes.         |
   * | `calibration.drift_velocity` | `double`    | mm/ns | 0.05             | Drift velocity assumed by the reconstruction.  |
   * | `calibration.wire_velocity`  | `double`    | mm/ns | 200              | Signal propagation speed along the wire.       |
   * | `calibration.tdc_resolution` | `double`    | ns    | 3.5              | TDC time resolution.                           |
   * | `calibration.t0_offset`      | `double`    | ns    | 0                | Fixed electronics offset.                      |
   * | `propagator.max_step`        | `double`    | mm    | 50               | Maximum Runge-Kutta step.                      |
   * | `propagator.max_slope`       | `double`    |       | 1000             | Propagation fails if \|tx\| or \|ty\| exceed it. |
   *
   * The `calibration` and `propagator` objects are optional, and so is each of their keys. Velocities, resolution,
   * step and slope must be positive; unknown keys are rejected, so that a misspelt key is not silently ignored.
   *
   * Example:
   * \code{.json}
   * "sand::tracker::kalman::engine" : {
   *   "field" : [0.6, 0.0, 0.0],
   *   "calibration" : { "drift_velocity" : 0.05, "wire_velocity" : 200.0, "tdc_resolution" : 3.5, "t0_offset" : 0.0 },
   *   "propagator" : { "max_step" : 50.0, "max_slope" : 1000.0 }
   * }
   * \endcode
   *
   * `field` is either an array `[Bx, By, Bz]`, a uniform field, or an object for the field of the geometry volumes
   * (sand::tracker::kalman::volume_field):
   * \code{.json}
   * "field" : { "source" : "gdml", "file" : "geometries/sand/SAND_opt3_DRIFT1.gdml" }
   * \endcode
   * The BField tags are read from the GDML `file` (resolved with ufw::config::path_at, so relative paths are relative
   * to `ufw-basepath`) and applied to the geometry of sand::root_tgeomanager, which must be configured in the same
   * job. `source` must be `"gdml"`. The GDML must be the one the geometry was made from.
   *
   * The engine is not meant to be used from several threads at once: the field of the geometry keeps a navigator and
   * a cache that are updated by every lookup.
   */
  class engine : ufw::data::base<ufw::data::complex_tag, ufw::data::unique_tag, ufw::data::global_tag> {
   public:
    /// Build the engine from its configuration (see the table above).
    explicit engine(const ufw::config&);
    ~engine();

    engine(const engine&)             = delete;
    engine& operator= (const engine&) = delete;

    /// Magnetic field used by propagate()
    const kalman::field& field() const { return *m_field; }
    /// What the reconstruction assumes about the detector response
    const reco_calibration& calibration() const { return m_calibration; }
    /// Runge-Kutta settings used by propagate()
    const propagator_config& propagator_settings() const { return m_propagator; }

    /**
     * @brief Propagate a state to the plane z_target with the field and the settings of the engine.
     * @param in       Input state (its covariance is transported)
     * @param z_target Target plane [mm]
     * @param mass     Mass hypothesis [MeV], used only for the time of flight
     * @return As sand::tracker::kalman::propagate()
     */
    std::optional<propagation_result> propagate(const state& in, double z_target, double mass) const;

    /**
     * @brief Predicted TDC time for a wire, with the calibration of the engine.
     * @param s    State on the wire plane
     * @param w    Wire
     * @param side Left-right hypothesis: +1 or -1
     * @return As sand::tracker::kalman::wire_time()
     */
    prediction wire_time(const state& s, const wire_geometry& w, int side) const;

   private:
    std::unique_ptr<kalman::field> m_field; ///< Owned field, behind the abstract interface
    reco_calibration m_calibration;         ///< Reconstruction calibration
    propagator_config m_propagator;         ///< Runge-Kutta settings
  };

} // namespace sand::tracker::kalman

UFW_DECLARE_COMPLEX_DATA(sand::tracker::kalman::engine);
