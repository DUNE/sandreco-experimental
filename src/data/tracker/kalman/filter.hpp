#pragma once

#include <kalman/measurement.hpp>
#include <kalman/state.hpp>

#include <optional>
#include <vector>

/**
 * @file filter.hpp
 * @brief Kalman update for a scalar measurement, and Rauch-Tung-Striebel smoother.
 *
 * The update uses the Joseph form of the covariance, which is much more robust under rounding than the short form
 * (I - K H) C, and symmetrises the result explicitly. A positive weight w scales the measurement variance as V / w:
 * this is the hook for the deterministic annealing filter, where each hypothesis (wire side) enters with its own
 * weight in (0, 1]. Weights above 1 are accepted and scale the variance down. A weight that is zero, negative or NaN
 * is rejected.
 *
 * The formulas are in docs/kalman_filter.md, sections "2.8 Gain K and update" and "2.9 Smoother".
 */

namespace sand::tracker::kalman {

  /// Output of update().
  struct update_result {
    state out;                ///< Updated state
    double residual{0};       ///< Measured minus predicted, before the update
    double chi2{0};           ///< Predicted chi2 increment r^2 / (H C H^T + V), with the unweighted V
    double residual_after{0}; ///< Measured minus predicted, after the update
  };

  /**
   * @brief Kalman update of a state with one scalar measurement.
   *
   * Gain K = C H^T / S with S = H C H^T + V / w, covariance in Joseph form. See docs/kalman_filter.md, section
   * "2.8 Gain K and update".
   *
   * Contract on the weight: weight > 0. The deterministic annealing filter uses weights in (0, 1]; a weight above 1 is
   * accepted too and scales the measurement variance down (V / w < V), as for a more precise measurement. A weight of
   * 0 would make V / w infinite and the covariance NaN, so a weight that is not positive (including NaN) throws instead
   * of failing silently. A caller that gives a measurement no weight at all (for example the deterministic annealing
   * filter, for a hypothesis below its minimum weight) must skip the update, not call it with weight 0.
   *
   * @param in        State on the measurement plane
   * @param pred      Predicted value and derivative row
   * @param measured  Measured value
   * @param variance  Measurement variance
   * @param weight    Weight of the measurement, > 0 (in (0, 1] for the DAF; above 1 scales V down). Defaults to 1, a
   *                  normal measurement.
   * @return Updated state, residuals and chi2 increment
   * @throws std::invalid_argument if weight is not positive or is NaN
   */
  update_result update(const state& in, const prediction& pred, double measured, double variance, double weight = 1.);

  /**
   * @brief One step of a forward filter pass, kept for the smoother.
   *
   * The smoother needs, at each plane k + 1, the predicted state x_{k+1|k} with its covariance, the filtered state
   * x_{k+1|k+1}, and the transport Jacobian F_{k+1} from plane k to plane k + 1. See docs/kalman_filter.md, section
   * "2.9 Smoother".
   */
  struct filter_step {
    state predicted; ///< Before the update at this plane
    state filtered;  ///< After the update at this plane
    matrix_t F;      ///< Transport from the previous filtered state to this plane
  };

  /**
   * @brief Rauch-Tung-Striebel smoother.
   *
   * Smoother gain G_k = C_{k|k} F_{k+1}^T (C_{k+1|k})^-1, then x_{k|N} and C_{k|N} backwards from the last plane N.
   * See docs/kalman_filter.md, section "2.9 Smoother".
   *
   * @param steps Forward filter steps in the order they were filtered; steps[0].F is not used.
   * @return Smoothed states, one per step, or nothing if a predicted covariance cannot be inverted.
   */
  std::optional<std::vector<state>> smooth(const std::vector<filter_step>& steps);

} // namespace sand::tracker::kalman
