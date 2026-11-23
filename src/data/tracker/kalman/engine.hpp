#pragma once

#include <kalman/field.hpp>
#include <kalman/material.hpp>
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
   * | `field`                      | `double[3]` | T     | Required         | Uniform magnetic field (Bx, By, Bz).           |
   * | `calibration.drift_velocity` | `double`    | mm/ns | 0.05             | Drift velocity assumed by the reconstruction.  |
   * | `calibration.wire_velocity`  | `double`    | mm/ns | 200              | Signal propagation speed along the wire.       |
   * | `calibration.tdc_resolution` | `double`    | ns    | 3.5              | TDC time resolution.                           |
   * | `calibration.t0_offset`      | `double`    | ns    | 0                | Fixed electronics offset.                      |
   * | `propagator.max_step`        | `double`    | mm    | 50               | Maximum Runge-Kutta step.                      |
   * | `propagator.max_slope`       | `double`    |       | 1000             | Propagation fails if \|tx\| or \|ty\| exceed it. |
   * | `material`                   | `string`    |       | `"none"`         | Material budget used by propagate().           |
   *
   * The `calibration` and `propagator` objects are optional, and so is each of their keys. Velocities, resolution,
   * step and slope must be positive; unknown keys are rejected, so that a misspelt key is not silently ignored.
   *
   * `material` is `"none"` (no material: propagate() without a budget is the field-only propagation) or
   * `"tracker_info"`: the engine takes `geoinfo::tracker_info` from the current context (so `sand::geoinfo` and
   * `sand::root_tgeomanager` must be configured) and owns a sand::tracker::kalman::station_material_budget built from
   * it. Any other value is rejected.
   *
   * Example:
   * \code{.json}
   * "sand::tracker::kalman::engine" : {
   *   "field" : [0.6, 0.0, 0.0],
   *   "calibration" : { "drift_velocity" : 0.05, "wire_velocity" : 200.0, "tdc_resolution" : 3.5, "t0_offset" : 0.0 },
   *   "propagator" : { "max_step" : 50.0, "max_slope" : 1000.0 },
   *   "material" : "tracker_info"
   * }
   * \endcode
   *
   * The field is uniform for now; a field map read from the GDML will be another implementation of the
   * sand::tracker::kalman::field interface, owned by the engine in the same way.
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
    /// Material budget owned by the engine, used by propagate() without an explicit budget; nullptr for "none"
    const material_budget* material() const { return m_material.get(); }

    /**
     * @brief Propagate a state to the plane z_target with the field, the material and the settings of the engine.
     * @param in       Input state (its covariance is transported)
     * @param z_target Target plane [mm]
     * @param mass     Mass hypothesis [MeV], used for the time of flight and, with material, for the energy loss and
     *                 the scattering angle
     * @return With `"material" : "none"`, as the field-only sand::tracker::kalman::propagate(); otherwise as its
     *         material overload with the budget of the engine (material()).
     */
    std::optional<propagation_result> propagate(const state& in, double z_target, double mass) const;

    /**
     * @brief Propagate a state to the plane z_target through the material, with the field and the settings of the
     * engine.
     * @param in       Input state (its covariance is transported)
     * @param z_target Target plane [mm]
     * @param mass     Mass hypothesis [MeV], used for the time of flight, the energy loss and the scattering angle
     * @param material Material budget, used instead of the one of the engine
     * @return As the material overload of sand::tracker::kalman::propagate()
     */
    std::optional<propagation_result> propagate(const state& in, double z_target, double mass,
                                                const material_budget& material) const;

    /**
     * @brief Predicted TDC time for a wire, with the calibration of the engine.
     * @param s    State on the wire plane
     * @param w    Wire
     * @param side Left-right hypothesis: +1 or -1
     * @return As sand::tracker::kalman::wire_time()
     */
    prediction wire_time(const state& s, const wire_geometry& w, int side) const;

   private:
    std::unique_ptr<kalman::field> m_field;      ///< Owned field, behind the abstract interface
    reco_calibration m_calibration;              ///< Reconstruction calibration
    propagator_config m_propagator;              ///< Runge-Kutta settings
    std::unique_ptr<material_budget> m_material; ///< Owned material budget; null for "none"
  };

} // namespace sand::tracker::kalman

UFW_DECLARE_COMPLEX_DATA(sand::tracker::kalman::engine);
