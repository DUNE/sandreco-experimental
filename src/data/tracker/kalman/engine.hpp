#pragma once

#include <kalman/daf.hpp>
#include <kalman/field.hpp>
#include <kalman/material.hpp>
#include <kalman/measurement.hpp>
#include <kalman/propagator.hpp>
#include <kalman/state.hpp>

#include <common/data.h>
#include <ufw/config.hpp>

#include <memory>
#include <optional>
#include <vector>

/**
 * @file engine.hpp
 * @brief Complex data object holding the state shared by the tracker reconstruction modules.
 */

namespace sand::tracker::kalman {

  /**
   * \class sand::tracker::kalman::engine
   *
   * \brief Shared state of the tracker Kalman filter: magnetic field, reconstruction calibration, propagation
   * settings and settings of the deterministic annealing filter.
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
   * The operations that need the shared state are available as members (propagate(), wire_time(), fit()). The free
   * functions of the library (sand::tracker::kalman::propagate(), sand::tracker::kalman::wire_time(),
   * sand::tracker::kalman::daf_fit(), ...) remain
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
   * | `daf.temperatures`           | `double[]`  |       | [81, 81, 9, 4, 1, 1] | Annealing schedule of the DAF.             |
   * | `daf.chi2_cut`               | `double`    |       | 9                | Cutoff chi2 of the DAF weights.                |
   * | `daf.max_iterations`         | `int`       |       | 10               | Maximum extra DAF passes at T = 1.             |
   * | `daf.convergence_threshold`  | `double`    |       | 0.001            | Largest change of a weight at convergence.     |
   * | `daf.min_weight`             | `double`    |       | 0.001            | Candidates not above it are left out.          |
   * | `daf.anneal_variance`        | `bool`      |       | true             | Update with variance T V / w (false: V / w).   |
   * | `daf.initial_weights`        | `string`    |       | "equal"          | First-pass weights: "equal" or "prediction".   |
   * | `daf.final_prior_scale`      | `double`    |       | 1e4              | Prior covariance factor of the final pass.     |
   * | `material`                   | `string`    |       | `"none"`         | Material used by propagate() and fit().        |
   *
   * The `calibration`, `propagator` and `daf` objects are optional, and so is each of their keys. Velocities,
   * resolution, step, slope, temperatures, cutoff and convergence threshold must be positive, max_iterations a
   * non-negative integer, min_weight in [0, 1), anneal_variance a boolean, initial_weights "equal" or "prediction"
   * and final_prior_scale at least 1 (1: no final pass); unknown keys are rejected, so that a misspelt key is not
   * silently ignored.
   *
   * `material` is `"none"` (no material: propagate() and fit() without a budget use the field-only propagation) or
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
   *   "daf" : { "temperatures" : [81, 81, 9, 4, 1, 1], "chi2_cut" : 9.0, "max_iterations" : 10,
   *             "convergence_threshold" : 0.001, "min_weight" : 0.001, "anneal_variance" : true,
   *             "initial_weights" : "equal", "final_prior_scale" : 1e4 },
   *   "material" : "tracker_info"
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
    /// Settings of the deterministic annealing filter used by fit()
    const daf_config& daf_settings() const { return m_daf; }
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

    /**
     * @brief Fit a track with the deterministic annealing filter, with the field, material, calibration, propagator
     * and DAF settings of the engine.
     * @param initial Initial state (prior) with its covariance
     * @param sites   Sites ordered along the motion of the particle
     * @param mass    Mass hypothesis [MeV]
     * @return As sand::tracker::kalman::daf_fit() with the budget of the engine (material()); with
     *         `"material" : "none"`, with an empty budget, i.e. field-only propagation between the sites.
     */
    daf_result fit(const state& initial, const std::vector<site>& sites, double mass) const;

    /**
     * @brief Fit a track with the deterministic annealing filter, with the field, calibration, propagator and DAF
     * settings of the engine and a material budget given by the caller instead of the engine's.
     * @param initial  Initial state (prior) with its covariance
     * @param sites    Sites ordered along the motion of the particle
     * @param mass     Mass hypothesis [MeV]
     * @param material Material budget used instead of material()
     * @return As sand::tracker::kalman::daf_fit()
     */
    daf_result fit(const state& initial, const std::vector<site>& sites, double mass,
                   const material_budget& material) const;

   private:
    std::unique_ptr<kalman::field> m_field;      ///< Owned field, behind the abstract interface
    reco_calibration m_calibration;              ///< Reconstruction calibration
    propagator_config m_propagator;              ///< Runge-Kutta settings
    daf_config m_daf;                            ///< Deterministic annealing filter settings
    std::unique_ptr<material_budget> m_material; ///< Owned material budget; null for "none"
  };

} // namespace sand::tracker::kalman

UFW_DECLARE_COMPLEX_DATA(sand::tracker::kalman::engine);
