#pragma once

#include <kalman/field.hpp>
#include <kalman/material.hpp>
#include <kalman/measurement.hpp>
#include <kalman/propagator.hpp>
#include <kalman/state.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

/**
 * @file daf.hpp
 * @brief Deterministic annealing filter (DAF) for the tracker: a track fit in which competing measurements (the two
 * sides of a wire, other digits on the same plane) enter with weights that are annealed from a high temperature down
 * to T = 1.
 *
 * Reference: R. Fruhwirth, A. Strandlie, Track fitting with ambiguities and noise: a study of elastic tracking and
 * nonlinear filters, Comput. Phys. Commun. 120 (1999) 197.
 *
 * See docs/kalman_filter.md, section "Deterministic annealing filter".
 */

namespace sand::tracker::kalman {

  /// One competing measurement on a site: a wire, its TDC and a left-right hypothesis.
  struct measurement_candidate {
    wire_geometry wire; ///< The wire; its plane must be the plane of the site
    double tdc{0};      ///< Measured TDC time [ns]
    int side{+1};       ///< Left-right hypothesis: +1 or -1, the sign of the distance of closest approach
    std::size_t id{0};  ///< Opaque identifier, not used by the fit (e.g. the index of the digit)
  };

  /**
   * @brief The candidates on one wire plane.
   *
   * A wire digit gives two candidates, one per side; other digits on the same plane are more candidates. At most
   * one candidate is expected to belong to the track; the DAF decides which by the weights.
   */
  struct site {
    double z{0};                                   ///< Plane [mm]
    std::vector<measurement_candidate> candidates; ///< Competing measurements, at least one
  };

  /// How the weights of the first pass are chosen.
  enum class daf_initial_weights {
    equal,      ///< 1/n for the n candidates of each site
    prediction, ///< From the prediction of the prior, propagated without updates, at the first temperature
  };

  /// Settings of the deterministic annealing filter.
  struct daf_config {
    /// Annealing schedule, one pass per temperature. The first pass uses the initial weights, so the first
    /// temperature is repeated for a pass with weights computed from a track.
    std::vector<double> temperatures{81., 81., 9., 4., 1., 1.};
    double chi2_cut{9.};                ///< Cutoff chi2: a candidate with chi2 above it is suppressed (9: 3 sigma)
    int max_iterations{10};             ///< Maximum number of extra passes at T = 1 after the schedule
    double convergence_threshold{1e-3}; ///< Converged when no weight changes by more than this in a pass
    double min_weight{1e-3};            ///< Candidates with a weight not above this are left out of the update
    bool anneal_variance{true};         ///< Measurement variance T V / w in the update (false: V / w)
    daf_initial_weights initial_weights{daf_initial_weights::equal}; ///< Weights of the first pass
    /// Factor on the prior covariance in the final pass at T = 1 with the converged weights fixed, which gives the
    /// result; 1: no final pass, the result is the last pass of the annealing
    double final_prior_scale{1e4};
  };

  /**
   * @brief Check a DAF configuration.
   * @param cfg The configuration
   * @return An explanation of the first problem found, or nothing if the configuration is valid: temperatures not
   *         empty and all positive, chi2_cut positive, max_iterations not negative, convergence_threshold positive,
   *         min_weight in [0, 1), final_prior_scale at least 1.
   */
  std::optional<std::string> check(const daf_config& cfg);

  /// Outcome of daf_fit().
  enum class daf_status {
    ok,                    ///< The fit succeeded
    invalid_config,        ///< The daf_config is not valid (see check())
    invalid_input,         ///< No sites, a site without candidates, sites not ordered along the motion, ...
    propagation_failed,    ///< A propagation failed (track too steep, or stopped in the material)
    not_positive_definite, ///< A covariance matrix lost positive definiteness
    no_measurements,       ///< In a pass every candidate of every site had a weight below the minimum
  };

  /**
   * @brief Result of daf_fit().
   *
   * With the final pass (final_prior_scale != 1), states, residuals, skipped_sites, chi2 and smoothed_chi2 come from
   * it, and weights are the converged weights it used.
   */
  struct daf_result {
    bool success{false};                          ///< True if status is daf_status::ok
    daf_status status{daf_status::invalid_input}; ///< What happened
    std::string reason;                       ///< Explanation of a failure (with the site and pass), empty on success
    std::vector<state> states;                ///< Smoothed state on each site, in the order of the sites
    std::vector<std::vector<double>> weights; ///< weights[k][i]: weight of candidate i of site k
    std::vector<std::size_t> skipped_sites;   ///< Sites with no candidate above min_weight in the last pass
    /// residuals[k][i]: measured minus predicted TDC of candidate i of site k on the smoothed state [ns]
    std::vector<std::vector<double>> residuals;
    /// Filter chi2 of the last forward pass: sum over the candidates used of w_i r_i^2 / (V + H C H^T), with r_i the
    /// residual before the update and C the covariance before it (V unscaled by the temperature)
    double chi2{0};
    /// Sum over all candidates of w_i r_i^2 / V with the smoothed residuals; lower than chi2, since the smoothed track
    /// follows part of the scattering
    double smoothed_chi2{0};
    double ndf{0};         ///< Sum of all the weights minus 6, the number of track parameters
    int iterations{0};     ///< Number of passes (filter and smoother) done, the final pass included
    bool converged{false}; ///< Whether the last pass changed no weight by more than the threshold
  };

  /**
   * @brief Fit a track with the deterministic annealing filter.
   * @param initial    Initial state (prior) with its covariance, on any plane; it is the starting point of every
   *                   forward pass. Its direction gives the direction of motion.
   * @param sites      Sites ordered along the motion of the particle (z increasing for direction +1, decreasing for
   *                   direction -1), strictly.
   * @param mass       Mass hypothesis [MeV]
   * @param bfield     Magnetic field
   * @param material   Material budget
   * @param cal        Reconstruction calibration (drift and wire velocity, TDC resolution, offset)
   * @param prop_cfg   Runge-Kutta settings
   * @param cfg        Annealing settings
   * @return The smoothed states and the weights; on failure, success is false with a status and a reason.
   *
   * For each temperature T of the schedule, one pass:
   *  - weights at T from the smoothed states of the previous pass (first pass: the initial weights, see below):
   *    chi2_i = r_i^2 / (T V)
   *    with r_i the residual of wire_time and V = tdc_resolution^2, phi_i = exp(-chi2_i / 2),
   *    w_i = phi_i / (sum_j phi_j + exp(-chi2_cut / (2T)));
   *  - forward filter: propagate site by site (with material) and update with every candidate whose weight is above
   *    the minimum, sequentially, with variance T V / w (V / w if anneal_variance is false);
   *  - Rauch-Tung-Striebel smoother.
   *
   * After the schedule, passes at T = 1 are repeated until the weights computed from the last pass differ from those
   * it used by no more than convergence_threshold, or max_iterations extra passes are done.
   *
   * Then, unless final_prior_scale is 1, a final pass at T = 1 with those weights fixed and the covariance of the
   * prior multiplied by final_prior_scale gives the states, the residuals and chi2 of the result. In the
   * reconstruction the seed is built from the same digits as the fit, so it is not an independent measurement:
   * inflating its covariance removes its information, and chi2 has mean ndf = sum of the weights - 6.
   *
   * The first pass uses equal weights 1/n for the n candidates of each site, or, with initial_weights = prediction,
   * the weights at the first temperature from the prior propagated to each site without updates, with the variance
   * of the prediction added: r_i^2 / (T V + H C H^T).
   *
   * In the first pass propagation and measurements are linearised at the predicted state (an extended Kalman
   * filter); in the following passes they are linearised around the smoothed track of the previous pass, while the
   * prior stays the same. With a single candidate per site, T = {1}, equal initial weights and weights that stay at 1,
   * the result is the plain filter and smoother.
   *
   * The candidates of a site are linearised at the same point, so the order in which they are applied does not
   * matter. A site where all the weights are below the minimum is crossed without update and listed in
   * skipped_sites; its weights are still recomputed after the pass.
   *
   * The time of the track is a parameter of the fit, nearly degenerate with the drift distances while the sides are
   * unknown: the seed must give the track time to a few ns. See docs/kalman_filter.md.
   */
  daf_result daf_fit(const state& initial, const std::vector<site>& sites, double mass, const field& bfield,
                     const material_budget& material, const reco_calibration& cal, const propagator_config& prop_cfg,
                     const daf_config& cfg);

  /**
   * @brief DAF weights of the candidates of one site.
   * @param chi2        chi2 of each candidate
   * @param temperature Annealing temperature T
   * @param chi2_cut    Cutoff chi2
   * @return w_i = exp(-chi2_i / (2T)) / (sum_j exp(-chi2_j / (2T)) + exp(-chi2_cut / (2T))), computed with a common
   *         scale factor so that large chi2 values do not underflow to 0 / 0. A non-finite chi2 gets weight 0.
   */
  std::vector<double> daf_weights(const std::vector<double>& chi2, double temperature, double chi2_cut);

} // namespace sand::tracker::kalman
