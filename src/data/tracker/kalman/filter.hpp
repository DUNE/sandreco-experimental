#pragma once

#include <kalman/measurement.hpp>
#include <kalman/state.hpp>

#include <optional>
#include <vector>

/**
 * @file filter.hpp
 * @brief Kalman update for a scalar measurement, and Rauch-Tung-Striebel smoother.
 *
 * The update uses the Joseph form of the covariance, which stays symmetric and positive definite under rounding.
 * A weight w in (0, 1] scales the measurement variance as V / w: this is the hook for the deterministic annealing
 * filter, where each hypothesis (wire side) enters with its own weight.
 *
 * The formulas are in docs/kalman_filter.md, sections "Kalman update" and "Smoother".
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
   * Gain K = C H^T / (H C H^T + V / w), covariance in Joseph form. See docs/kalman_filter.md, section "Kalman update".
   *
   * @param in        State on the measurement plane
   * @param pred      Predicted value and derivative row
   * @param measured  Measured value
   * @param variance  Measurement variance
   * @param weight    DAF weight, 0 < weight <= 1
   * @return Updated state, residuals and chi2 increment
   */
  update_result update(const state& in, const prediction& pred, double measured, double variance, double weight = 1.);

  /**
   * @brief One step of a forward filter pass, kept for the smoother.
   *
   * The smoother needs, at each plane k + 1, the predicted state x_{k+1|k} with its covariance, the filtered state
   * x_{k+1|k+1}, and the transport Jacobian F_{k+1} from plane k to plane k + 1. See docs/kalman_filter.md, section
   * "Smoother".
   */
  struct filter_step {
    state predicted; ///< Before the update at this plane
    state filtered;  ///< After the update at this plane
    matrix_t F;      ///< Transport from the previous filtered state to this plane
  };

  /**
   * @brief Rauch-Tung-Striebel smoother.
   *
   * Smoother gain A_k = C_k|k F_{k+1}^T (C_{k+1|k})^-1, then x_k|n and C_k|n backwards from the last plane. See
   * docs/kalman_filter.md, section "Smoother".
   *
   * @param steps Forward filter steps in the order they were filtered; steps[0].F is not used.
   * @return Smoothed states, one per step, or nothing if a predicted covariance cannot be inverted.
   */
  std::optional<std::vector<state>> smooth(const std::vector<filter_step>& steps);

} // namespace sand::tracker::kalman
