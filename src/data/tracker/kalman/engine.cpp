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

    /// The object `key` of cfg, which must be present.
    const ufw::json& required_object(const ufw::json& cfg, const char* key) {
      if (!cfg.contains(key)) {
        UFW_ERROR("sand::tracker::kalman::engine: missing required key '{}'.", key);
      }
      return cfg.at(key);
    }

    /// The number `where.key` of cfg, which must be present: the library has no default values.
    double required_number(const ufw::json& cfg, std::string_view where, const char* key) {
      if (!cfg.contains(key)) {
        UFW_ERROR("sand::tracker::kalman::engine: missing required key '{}.{}'.", where, key);
      }
      auto const& v = cfg.at(key);
      if (!v.is_number()) {
        UFW_ERROR("sand::tracker::kalman::engine: '{}.{}' must be a number, got {}.", where, key, v.dump());
      }
      return v.get<double>();
    }

    /// As required_number(), and the value must be positive.
    double required_positive(const ufw::json& cfg, std::string_view where, const char* key) {
      double const v = required_number(cfg, where, key);
      if (!(v > 0.)) {
        UFW_ERROR("sand::tracker::kalman::engine: '{}.{}' must be positive, got {}.", where, key, v);
      }
      return v;
    }

  } // namespace

  engine::engine(const ufw::config& cfg) {
    check_keys(cfg, "top level", {"field", "calibration", "propagator"});

    // Field in tesla, as a plain number (see state.hpp)
    if (!cfg.contains("field")) {
      UFW_ERROR("sand::tracker::kalman::engine: missing required key 'field'.");
    }
    auto const& f = cfg.at("field");
    if (!f.is_array() || f.size() != 3) {
      UFW_ERROR("sand::tracker::kalman::engine: 'field' must be an array of three numbers [T], got {}.", f.dump());
    }
    m_field = std::make_unique<uniform_field>(f.get<field_vector>());

    // Every tunable parameter is required: neither the engine nor the structs have default values.
    auto const& c = required_object(cfg, "calibration");
    check_keys(c, "calibration", {"drift_velocity", "wire_velocity", "time_resolution", "t0_offset"});
    m_calibration = reco_calibration{
        required_positive(c, "calibration", "drift_velocity"), required_positive(c, "calibration", "wire_velocity"),
        required_positive(c, "calibration", "time_resolution"), required_number(c, "calibration", "t0_offset")};

    auto const& p = required_object(cfg, "propagator");
    check_keys(p, "propagator", {"max_step", "max_slope"});
    m_propagator = propagator_config{required_positive(p, "propagator", "max_step"),
                                     required_positive(p, "propagator", "max_slope")};

    auto const b = m_field->at(0., 0., 0.);
    UFW_INFO("sand::tracker::kalman::engine: uniform field ({}, {}, {}) T, drift velocity {} mm/ns, wire velocity {} "
             "mm/ns, time resolution {} ns, t0 offset {} ns, max step {} mm, max slope {}.",
             b[0], b[1], b[2], m_calibration.drift_velocity, m_calibration.wire_velocity, m_calibration.time_resolution,
             m_calibration.t0_offset, m_propagator.max_step, m_propagator.max_slope);
  }

  engine::~engine() = default;

  std::optional<propagation_result> engine::propagate(const state& in, double z_target,
                                                      const particle_hypothesis& hyp) const {
    return kalman::propagate(in, z_target, hyp, *m_field, m_propagator);
  }

  prediction engine::wire_time(const state& s, const wire_geometry& w, int side) const {
    return kalman::wire_time(s, w, side, m_calibration);
  }

} // namespace sand::tracker::kalman
