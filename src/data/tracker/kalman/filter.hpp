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
 */

namespace sand::tracker::kalman {

  /// Output of update().
  struct update_result {
    state out;                ///< Updated state
    double residual{0};       ///< Measured minus predicted, before the update
    double chi2{0};           ///< Predicted chi2 increment r^2 / (H C H^T + V)
    double residual_after{0}; ///< Measured minus predicted, after the update
  };

  /**
   * @brief Kalman update of a state with one scalar measurement.
   * @param in        State on the measurement plane
   * @param pred      Predicted value and derivative row
   * @param measured  Measured value
   * @param variance  Measurement variance
   * @param weight    DAF weight, 0 < weight <= 1
   * @return Updated state, residuals and chi2 increment
   */
  update_result update(const state& in, const prediction& pred, double measured, double variance, double weight = 1.);

  /// One step of a forward filter pass, kept for the smoother.
  struct filter_step {
    state predicted; ///< Before the update at this plane
    state filtered;  ///< After the update at this plane
    matrix_t F;      ///< Transport from the previous filtered state to this plane
  };

  /**
   * @brief Rauch-Tung-Striebel smoother.
   * @param steps Forward filter steps in the order they were filtered; steps[0].F is not used.
   * @return Smoothed states, one per step, or nothing if a predicted covariance cannot be inverted.
   */
  std::optional<std::vector<state>> smooth(const std::vector<filter_step>& steps);

} // namespace sand::tracker::kalman
