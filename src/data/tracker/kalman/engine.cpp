#include <kalman/engine.hpp>

#include <ufw/config.hpp>

#include <initializer_list>
#include <string_view>

namespace sand::tracker::kalman {

  namespace {

    /// Throw if cfg is not an object or has a key that is not in allowed.
    void check_keys(const ufw::json& cfg, std::string_view where, std::initializer_list<std::string_view> allowed) {
      if (!cfg.is_object()) {
        UFW_ERROR("The {} configuration of sand::tracker::kalman::engine must be an object, got {}.", where,
                  cfg.dump());
      }
      for (auto const& item : cfg.items()) {
        bool known = false;
        for (auto const& a : allowed) {
          known = known || item.key() == a;
        }
        if (!known) {
          UFW_ERROR("Unknown key '{}' in the {} configuration of sand::tracker::kalman::engine.", item.key(), where);
        }
      }
    }

    /// Read an optional positive number, keeping the default if the key is absent.
    double positive(const ufw::json& cfg, const char* key, double def) {
      double const v = cfg.value(key, def);
      if (!(v > 0.)) {
        UFW_ERROR("sand::tracker::kalman::engine: '{}' must be positive, got {}.", key, v);
      }
      return v;
    }

  } // namespace

  engine::engine(const ufw::config& cfg) {
    check_keys(cfg, "top level", {"field", "calibration", "propagator", "daf"});

    if (!cfg.contains("field")) {
      UFW_ERROR("sand::tracker::kalman::engine: missing required key 'field'.");
    }
    auto const& f = cfg.at("field");
    if (!f.is_array() || f.size() != 3) {
      UFW_ERROR("sand::tracker::kalman::engine: 'field' must be an array of three numbers [T], got {}.", f.dump());
    }
    m_field = std::make_unique<uniform_field>(f.get<field_vector>());

    if (cfg.contains("calibration")) {
      auto const& c = cfg.at("calibration");
      check_keys(c, "calibration", {"drift_velocity", "wire_velocity", "tdc_resolution", "t0_offset"});
      m_calibration.drift_velocity = positive(c, "drift_velocity", m_calibration.drift_velocity);
      m_calibration.wire_velocity  = positive(c, "wire_velocity", m_calibration.wire_velocity);
      m_calibration.tdc_resolution = positive(c, "tdc_resolution", m_calibration.tdc_resolution);
      m_calibration.t0_offset      = c.value("t0_offset", m_calibration.t0_offset);
    }

    if (cfg.contains("propagator")) {
      auto const& p = cfg.at("propagator");
      check_keys(p, "propagator", {"max_step", "max_slope"});
      m_propagator.max_step  = positive(p, "max_step", m_propagator.max_step);
      m_propagator.max_slope = positive(p, "max_slope", m_propagator.max_slope);
    }

    if (cfg.contains("daf")) {
      auto const& d = cfg.at("daf");
      check_keys(d, "daf",
                 {"temperatures", "chi2_cut", "max_iterations", "convergence_threshold", "min_weight",
                  "anneal_variance", "initial_weights", "final_prior_scale"});
      if (d.contains("temperatures")) {
        auto const& t = d.at("temperatures");
        if (!t.is_array() || t.empty()) {
          UFW_ERROR("sand::tracker::kalman::engine: 'temperatures' must be a non-empty array of numbers, got {}.",
                    t.dump());
        }
        m_daf.temperatures.clear();
        for (auto const& x : t) {
          if (!x.is_number()) {
            UFW_ERROR("sand::tracker::kalman::engine: 'temperatures' must be a non-empty array of numbers, got {}.",
                      t.dump());
          }
          m_daf.temperatures.push_back(x.get<double>());
        }
      }
      m_daf.chi2_cut = positive(d, "chi2_cut", m_daf.chi2_cut);
      if (d.contains("max_iterations")) {
        auto const& m = d.at("max_iterations");
        if (!m.is_number_integer() || m.get<long long>() < 0) {
          UFW_ERROR("sand::tracker::kalman::engine: 'max_iterations' must be a non-negative integer, got {}.",
                    m.dump());
        }
        m_daf.max_iterations = m.get<int>();
      }
      m_daf.convergence_threshold = positive(d, "convergence_threshold", m_daf.convergence_threshold);
      m_daf.min_weight            = d.value("min_weight", m_daf.min_weight);
      m_daf.final_prior_scale     = positive(d, "final_prior_scale", m_daf.final_prior_scale);
      if (d.contains("anneal_variance")) {
        auto const& a = d.at("anneal_variance");
        if (!a.is_boolean()) {
          UFW_ERROR("sand::tracker::kalman::engine: 'anneal_variance' must be true or false, got {}.", a.dump());
        }
        m_daf.anneal_variance = a.get<bool>();
      }
      if (d.contains("initial_weights")) {
        auto const& iw = d.at("initial_weights");
        if (iw == "equal") {
          m_daf.initial_weights = daf_initial_weights::equal;
        } else if (iw == "prediction") {
          m_daf.initial_weights = daf_initial_weights::prediction;
        } else {
          UFW_ERROR("sand::tracker::kalman::engine: 'initial_weights' must be \"equal\" or \"prediction\", got {}.",
                    iw.dump());
        }
      }
      if (auto const problem = check(m_daf)) {
        UFW_ERROR("sand::tracker::kalman::engine: invalid 'daf' configuration: {}.", *problem);
      }
    }

    auto const b = m_field->at(0., 0., 0.);
    UFW_INFO("sand::tracker::kalman::engine: uniform field ({}, {}, {}) T, drift velocity {} mm/ns, wire velocity {} "
             "mm/ns, TDC resolution {} ns, t0 offset {} ns, max step {} mm, max slope {}.",
             b[0], b[1], b[2], m_calibration.drift_velocity, m_calibration.wire_velocity, m_calibration.tdc_resolution,
             m_calibration.t0_offset, m_propagator.max_step, m_propagator.max_slope);
    UFW_INFO(
        "sand::tracker::kalman::engine: DAF with {} temperatures (first {}, last {}), chi2 cut {}, at most {} extra "
        "passes, convergence threshold {}, minimum weight {}, variance annealing {}, {} initial weights, final prior "
        "scale {}.",
        m_daf.temperatures.size(), m_daf.temperatures.front(), m_daf.temperatures.back(), m_daf.chi2_cut,
        m_daf.max_iterations, m_daf.convergence_threshold, m_daf.min_weight, m_daf.anneal_variance ? "on" : "off",
        m_daf.initial_weights == daf_initial_weights::prediction ? "prediction" : "equal", m_daf.final_prior_scale);
  }

  engine::~engine() = default;

  std::optional<propagation_result> engine::propagate(const state& in, double z_target, double mass) const {
    return kalman::propagate(in, z_target, mass, *m_field, m_propagator);
  }

  std::optional<propagation_result> engine::propagate(const state& in, double z_target, double mass,
                                                      const material_budget& material) const {
    return kalman::propagate(in, z_target, mass, *m_field, material, m_propagator);
  }

  prediction engine::wire_time(const state& s, const wire_geometry& w, int side) const {
    return kalman::wire_time(s, w, side, m_calibration);
  }

  daf_result engine::fit(const state& initial, const std::vector<site>& sites, double mass,
                         const material_budget& material) const {
    return daf_fit(initial, sites, mass, *m_field, material, m_calibration, m_propagator, m_daf);
  }

} // namespace sand::tracker::kalman
