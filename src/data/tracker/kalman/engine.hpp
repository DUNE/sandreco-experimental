#pragma once

#include <kalman/field.hpp>
#include <kalman/measurement.hpp>
#include <kalman/particle.hpp>
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
   * auto prop = kf.propagate(current, w.z(), mass_from_pdg(13));
   * \endcode
   *
   * The operations that need the shared state are available as members (propagate(), wire_time()). The free
   * functions of the library (sand::tracker::kalman::propagate(), sand::tracker::kalman::wire_time(), ...) remain
   * available with explicit arguments; the members call them.
   *
   * The measurement model applies to drift chambers and straw tubes alike.
   *
   * \par Configuration
   * Key `"sand::tracker::kalman::engine"` in the `"globals"` section of the job.
   *
   * | Parameter Name                | Type        | Unit  | Required/Default | Description                                      |
   * |-------------------------------|-------------|-------|------------------|--------------------------------------------------|
   * | `field`                       | `double[3]` | T     | Required         | Uniform magnetic field (Bx, By, Bz).             |
   * | `calibration.drift_velocity`  | `double`    | mm/ns | Required         | Drift velocity assumed by the reconstruction.    |
   * | `calibration.wire_velocity`   | `double`    | mm/ns | Required         | Signal propagation speed along the wire.         |
   * | `calibration.time_resolution` | `double`    | ns    | Required         | Total resolution on the measured time (*).       |
   * | `calibration.t0_offset`       | `double`    | ns    | Required         | Fixed electronics offset.                        |
   * | `propagator.max_step`         | `double`    | mm    | Required         | Maximum Runge-Kutta step.                        |
   * | `propagator.max_slope`        | `double`    |       | Required         | Propagation fails if \|tx\| or \|ty\| exceed it. |
   *
   * (*) TDC resolution plus the intrinsic spatial resolution divided by the drift velocity, in quadrature; its square
   * is the measurement variance of the time update. With the current fast digitizations, which smear only the TDC, it
   * equals their `sigma_tdc`. See reco_calibration::time_resolution.
   *
   * Every key is required: every tunable parameter comes from the configuration, and neither the engine nor the
   * structs reco_calibration and propagator_config have default values. A missing key is an error that names it.
   * Velocities, resolution, step and slope must be positive; unknown keys are rejected, so that a misspelt key is not
   * silently ignored. The field is a number in tesla (see state.hpp).
   *
   * Example:
   * \code{.json}
   * "sand::tracker::kalman::engine" : {
   *   "field" : [0.6, 0.0, 0.0],
   *   "calibration" : { "drift_velocity" : 0.05, "wire_velocity" : 200.0, "time_resolution" : 3.5, "t0_offset" : 0.0 },
   *   "propagator" : { "max_step" : 50.0, "max_slope" : 1000.0 }
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

    /**
     * @brief Propagate a state to the plane z_target with the field and the settings of the engine.
     * @param in       Input state (its covariance is transported)
     * @param z_target Target plane [mm]
     * @param mass     Mass hypothesis [MeV], used only for the time of flight (see mass_from_pdg())
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
