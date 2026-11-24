#include <kalman/daf.hpp>
#include <kalman/filter.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <utility>

namespace sand::tracker::kalman {

  namespace {

    /// Largest distance [mm] allowed between the plane of a site and the plane of one of its wires.
    constexpr double s_plane_tolerance = 1e-3;

    /// Whether a symmetric matrix is positive definite, by Cholesky decomposition.
    bool positive_definite(const sym_matrix_t& c) {
      double l[s_dim][s_dim] = {};
      for (unsigned int j = 0; j < s_dim; ++j) {
        double d = c(j, j);
        for (unsigned int k = 0; k < j; ++k) {
          d -= l[j][k] * l[j][k];
        }
        if (!(d > 0.)) {
          return false;
        }
        l[j][j] = std::sqrt(d);
        for (unsigned int i = j + 1; i < s_dim; ++i) {
          double s = c(i, j);
          for (unsigned int k = 0; k < j; ++k) {
            s -= l[i][k] * l[j][k];
          }
          l[i][j] = s / l[j][j];
        }
      }
      return true;
    }

    /// Fixed inputs of a fit, shared by all passes.
    struct fit_inputs {
      const state& initial;
      const std::vector<site>& sites;
      double mass;
      const field& bfield;
      const material_budget& material;
      const reco_calibration& cal;
      const propagator_config& prop_cfg;
      const daf_config& cfg;
    };

    /// Output of one pass of filter and smoother.
    struct pass_output {
      daf_status status{daf_status::ok};
      std::string reason;
      std::vector<state> smoothed;
      std::vector<std::size_t> skipped;
      double filter_chi2{0}; ///< Sum of w r_pred^2 / (V + H C H^T) over the candidates used
    };

    pass_output pass_failure(daf_status s, std::string reason) {
      pass_output out;
      out.status = s;
      out.reason = std::move(reason);
      return out;
    }

    /**
     * One forward filter pass at temperature T with the given weights, followed by the smoother. The measurement
     * variance in the update is T V / w with annealing of the variance, V / w without. Without a reference the
     * propagation and the measurements are linearised along the pass itself (extended Kalman filter); with a
     * reference (the smoothed states of the previous pass) they are linearised around it.
     */
    pass_output run_pass(const fit_inputs& in, const std::vector<std::vector<double>>& weights,
                         const std::vector<state>* reference, double temperature) {
      auto const& sites     = in.sites;
      double const variance = in.cal.tdc_resolution * in.cal.tdc_resolution;
      double const update_variance = in.cfg.anneal_variance ? temperature * variance : variance;
      state current         = in.initial; // filtered state on the previous plane
      vector_t current_ref{};             // reference parameters on the previous plane
      if (reference) {
        if (in.initial.z == sites.front().z) {
          current_ref = reference->front().par;
        } else {
          auto const back = propagate(reference->front(), in.initial.z, in.mass, in.bfield, in.material, in.prop_cfg);
          if (!back) {
            return pass_failure(daf_status::propagation_failed,
                                "propagation of the reference from the first site to the initial plane failed");
          }
          current_ref = back->out.par;
        }
      }

      pass_output out;
      std::vector<filter_step> steps;
      steps.reserve(sites.size());
      for (std::size_t k = 0; k < sites.size(); ++k) {
        state base = current;
        if (reference) {
          base.par = current_ref;
        }
        auto const prop = propagate(base, sites[k].z, in.mass, in.bfield, in.material, in.prop_cfg);
        if (!prop) {
          return pass_failure(daf_status::propagation_failed, "propagation to site " + std::to_string(k) + " failed");
        }
        state predicted = prop->out;
        if (reference) {
          predicted.par = prop->out.par + prop->F * (current.par - current_ref);
        }

        // All the candidates of the site are linearised at the same point.
        state lin = predicted;
        if (reference) {
          lin.par = (*reference)[k].par;
        }
        state filtered = predicted;
        bool used      = false;
        for (std::size_t i = 0; i < sites[k].candidates.size(); ++i) {
          double const w = weights[k][i];
          if (!(w > in.cfg.min_weight)) {
            continue;
          }
          auto const& c = sites[k].candidates[i];
          prediction p  = wire_time(lin, c.wire, c.side, in.cal);
          p.value += ROOT::Math::Dot(p.H, filtered.par - lin.par);
          double const r   = c.tdc - p.value;
          double const hch = ROOT::Math::Dot(p.H, filtered.cov * p.H);
          out.filter_chi2 += w * r * r / (variance + hch);
          filtered = update(filtered, p, c.tdc, update_variance, w).out;
          used     = true;
        }
        if (!used) {
          out.skipped.push_back(k);
        }
        if (!positive_definite(filtered.cov)) {
          return pass_failure(daf_status::not_positive_definite,
                              "filtered covariance on site " + std::to_string(k) + " is not positive definite");
        }
        steps.push_back({predicted, filtered, prop->F});
        current = filtered;
        if (reference) {
          current_ref = (*reference)[k].par;
        }
      }
      if (out.skipped.size() == sites.size()) {
        return pass_failure(daf_status::no_measurements, "no candidate above the minimum weight on any site");
      }

      auto smoothed = smooth(steps);
      if (!smoothed) {
        return pass_failure(daf_status::not_positive_definite,
                            "a predicted covariance could not be inverted in the smoother");
      }
      for (std::size_t k = 0; k < smoothed->size(); ++k) {
        if (!positive_definite((*smoothed)[k].cov)) {
          return pass_failure(daf_status::not_positive_definite,
                              "smoothed covariance on site " + std::to_string(k) + " is not positive definite");
        }
      }
      out.smoothed = std::move(*smoothed);
      return out;
    }

    /// Residual (measured minus predicted TDC) of every candidate on the smoothed states.
    std::vector<std::vector<double>> candidate_residuals(const fit_inputs& in, const std::vector<state>& smoothed) {
      std::vector<std::vector<double>> r(in.sites.size());
      for (std::size_t k = 0; k < in.sites.size(); ++k) {
        for (auto const& c : in.sites[k].candidates) {
          r[k].push_back(c.tdc - wire_time(smoothed[k], c.wire, c.side, in.cal).value);
        }
      }
      return r;
    }

    /// DAF weights at temperature T from the residuals on the smoothed states.
    std::vector<std::vector<double>> weights_from_residuals(const fit_inputs& in,
                                                            const std::vector<std::vector<double>>& residuals,
                                                            double temperature) {
      double const variance = in.cal.tdc_resolution * in.cal.tdc_resolution;
      std::vector<std::vector<double>> w(residuals.size());
      for (std::size_t k = 0; k < residuals.size(); ++k) {
        std::vector<double> chi2;
        for (double r : residuals[k]) {
          chi2.push_back(r * r / variance);
        }
        w[k] = daf_weights(chi2, temperature, in.cfg.chi2_cut);
      }
      return w;
    }

    /// Largest absolute difference between two sets of weights.
    double weight_change(const std::vector<std::vector<double>>& a, const std::vector<std::vector<double>>& b) {
      double change = 0.;
      for (std::size_t k = 0; k < a.size(); ++k) {
        for (std::size_t i = 0; i < a[k].size(); ++i) {
          change = std::max(change, std::abs(a[k][i] - b[k][i]));
        }
      }
      return change;
    }

    /**
     * First-pass weights from the prediction of the prior, propagated without updates, at temperature T: the weight
     * formula with r^2 / (T V + H C H^T) in place of r^2 / (T V), C the covariance of the prediction.
     */
    pass_output prediction_weights(const fit_inputs& in, double temperature,
                                   std::vector<std::vector<double>>& weights) {
      double const variance = in.cal.tdc_resolution * in.cal.tdc_resolution;
      state current         = in.initial;
      for (std::size_t k = 0; k < in.sites.size(); ++k) {
        auto const prop = propagate(current, in.sites[k].z, in.mass, in.bfield, in.material, in.prop_cfg);
        if (!prop) {
          return pass_failure(daf_status::propagation_failed,
                              "propagation of the prior to site " + std::to_string(k) + " failed");
        }
        current = prop->out;
        std::vector<double> chi2;
        for (auto const& c : in.sites[k].candidates) {
          auto const p     = wire_time(current, c.wire, c.side, in.cal);
          double const r   = c.tdc - p.value;
          double const hch = ROOT::Math::Dot(p.H, current.cov * p.H);
          // daf_weights divides by 2T: this gives exp(-r^2 / (2 (T V + H C H^T))).
          chi2.push_back(temperature * r * r / (temperature * variance + hch));
        }
        weights[k] = daf_weights(chi2, temperature, in.cfg.chi2_cut);
      }
      return {};
    }

    /// Explanation of the first problem in the input, if any.
    std::optional<std::string> check_input(const state& initial, const std::vector<site>& sites) {
      if (initial.direction != +1 && initial.direction != -1) {
        return "the direction of the initial state must be +1 or -1";
      }
      if (sites.empty()) {
        return "no sites";
      }
      for (std::size_t k = 0; k < sites.size(); ++k) {
        auto const& s = sites[k];
        if (s.candidates.empty()) {
          return "site " + std::to_string(k) + " has no candidates";
        }
        for (auto const& c : s.candidates) {
          if (c.side != +1 && c.side != -1) {
            return "a candidate of site " + std::to_string(k) + " has side " + std::to_string(c.side)
                 + ", not +1 or -1";
          }
          if (!(std::abs(c.wire.z() - s.z) <= s_plane_tolerance)) {
            std::ostringstream m;
            m << "a wire of site " << k << " is at z = " << c.wire.z() << " mm, not on the plane of the site (" << s.z
              << " mm)";
            return m.str();
          }
        }
        if (k > 0 && !((s.z - sites[k - 1].z) * initial.direction > 0.)) {
          return "sites " + std::to_string(k - 1) + " and " + std::to_string(k)
               + " are not ordered along the motion of the particle";
        }
      }
      return std::nullopt;
    }

  } // namespace

  std::optional<std::string> check(const daf_config& cfg) {
    if (cfg.temperatures.empty()) {
      return "the annealing schedule has no temperatures";
    }
    for (double t : cfg.temperatures) {
      if (!(t > 0.)) {
        return "temperatures must be positive, got " + std::to_string(t);
      }
    }
    if (!(cfg.chi2_cut > 0.)) {
      return "chi2_cut must be positive, got " + std::to_string(cfg.chi2_cut);
    }
    if (cfg.max_iterations < 0) {
      return "max_iterations must not be negative, got " + std::to_string(cfg.max_iterations);
    }
    if (!(cfg.convergence_threshold > 0.)) {
      return "convergence_threshold must be positive, got " + std::to_string(cfg.convergence_threshold);
    }
    if (!(cfg.min_weight >= 0. && cfg.min_weight < 1.)) {
      return "min_weight must be in [0, 1), got " + std::to_string(cfg.min_weight);
    }
    if (!(cfg.final_prior_scale >= 1.)) {
      return "final_prior_scale must be at least 1, got " + std::to_string(cfg.final_prior_scale);
    }
    return std::nullopt;
  }

  std::vector<double> daf_weights(const std::vector<double>& chi2, double temperature, double chi2_cut) {
    double const minus_inf = -std::numeric_limits<double>::infinity();
    std::vector<double> a(chi2.size());
    double const a_cut = -chi2_cut / (2. * temperature);
    double top         = a_cut;
    for (std::size_t i = 0; i < chi2.size(); ++i) {
      a[i] = std::isfinite(chi2[i]) ? -chi2[i] / (2. * temperature) : minus_inf;
      top  = std::max(top, a[i]);
    }
    std::vector<double> w(chi2.size(), 0.);
    if (top == minus_inf) {
      return w;
    }
    double sum = std::exp(a_cut - top);
    for (std::size_t i = 0; i < a.size(); ++i) {
      w[i] = std::exp(a[i] - top);
      sum += w[i];
    }
    for (auto& x : w) {
      x /= sum;
    }
    return w;
  }

  daf_result daf_fit(const state& initial, const std::vector<site>& sites, double mass, const field& bfield,
                     const material_budget& material, const reco_calibration& cal, const propagator_config& prop_cfg,
                     const daf_config& cfg) {
    daf_result res;
    auto fail = [&res](daf_status s, std::string reason) {
      res.success = false;
      res.status  = s;
      res.reason  = std::move(reason);
      res.states.clear();
      return res;
    };

    if (auto const problem = check(cfg)) {
      return fail(daf_status::invalid_config, *problem);
    }
    if (auto const problem = check_input(initial, sites)) {
      return fail(daf_status::invalid_input, *problem);
    }
    if (!positive_definite(initial.cov)) {
      return fail(daf_status::not_positive_definite, "the covariance of the initial state is not positive definite");
    }

    fit_inputs const in{initial, sites, mass, bfield, material, cal, prop_cfg, cfg};
    double const variance = cal.tdc_resolution * cal.tdc_resolution;

    // Weights of the first pass: equal (1/n on the n candidates of a site) or from the prediction of the prior.
    std::vector<std::vector<double>> weights(sites.size());
    if (cfg.initial_weights == daf_initial_weights::prediction) {
      auto const out = prediction_weights(in, cfg.temperatures.front(), weights);
      if (out.status != daf_status::ok) {
        return fail(out.status, "first-pass weights: " + out.reason);
      }
    } else {
      for (std::size_t k = 0; k < sites.size(); ++k) {
        weights[k].assign(sites[k].candidates.size(), 1. / sites[k].candidates.size());
      }
    }

    // One pass of filter and smoother at temperature T with the current weights.
    auto do_pass = [&](double temperature, const fit_inputs& inputs) {
      auto out = run_pass(inputs, weights, res.iterations > 0 ? &res.states : nullptr, temperature);
      ++res.iterations;
      if (out.status != daf_status::ok) {
        fail(out.status, "pass " + std::to_string(res.iterations) + ": " + out.reason);
        return false;
      }
      res.states        = std::move(out.smoothed);
      res.skipped_sites = std::move(out.skipped);
      res.chi2          = out.filter_chi2;
      res.residuals     = candidate_residuals(in, res.states);
      return true;
    };

    // One pass per temperature of the schedule: the first with the initial weights, the others with the weights
    // computed at their temperature from the previous pass.
    for (std::size_t p = 0; p < cfg.temperatures.size(); ++p) {
      if (p > 0) {
        weights = weights_from_residuals(in, res.residuals, cfg.temperatures[p]);
      }
      if (!do_pass(cfg.temperatures[p], in)) {
        return res;
      }
    }
    // Then at T = 1 until the weights computed from the last pass are those it used.
    auto next     = weights_from_residuals(in, res.residuals, 1.);
    double change = weight_change(next, weights);
    for (int extra = 0; !(change < cfg.convergence_threshold) && extra < cfg.max_iterations; ++extra) {
      weights = next;
      if (!do_pass(1., in)) {
        return res;
      }
      next   = weights_from_residuals(in, res.residuals, 1.);
      change = weight_change(next, weights);
    }

    res.converged = change < cfg.convergence_threshold;
    res.weights   = std::move(next);

    // Final pass at T = 1 with the converged weights fixed and the covariance of the prior inflated, so that the seed
    // (built from the same digits) does not count as an independent measurement.
    if (cfg.final_prior_scale != 1.) {
      state diffuse = initial;
      diffuse.cov *= cfg.final_prior_scale;
      fit_inputs const final_in{diffuse, sites, mass, bfield, material, cal, prop_cfg, cfg};
      weights = res.weights;
      if (!do_pass(1., final_in)) {
        return res;
      }
    }

    double sum_w      = 0.;
    res.smoothed_chi2 = 0.;
    for (std::size_t k = 0; k < sites.size(); ++k) {
      for (std::size_t i = 0; i < res.weights[k].size(); ++i) {
        res.smoothed_chi2 += res.weights[k][i] * res.residuals[k][i] * res.residuals[k][i] / variance;
        sum_w += res.weights[k][i];
      }
    }
    res.ndf     = sum_w - static_cast<double>(s_dim);
    res.success = true;
    res.status  = daf_status::ok;
    return res;
  }

} // namespace sand::tracker::kalman
