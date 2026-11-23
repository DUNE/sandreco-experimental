#include <kalman/engine.hpp>
#include <kalman/station_material.hpp>

#include <geoinfo/geoinfo.hpp>
#include <ufw/config.hpp>
#include <ufw/context.hpp>

#include <initializer_list>
#include <memory>
#include <string>
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
    check_keys(cfg, "top level", {"field", "calibration", "propagator", "material"});

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

    std::string material = "none";
    if (cfg.contains("material")) {
      auto const& m = cfg.at("material");
      if (!m.is_string()) {
        UFW_ERROR("sand::tracker::kalman::engine: 'material' must be \"none\" or \"tracker_info\", got {}.", m.dump());
      }
      material = m.get<std::string>();
    }
    if (material == "tracker_info") {
      auto const& gi = ufw::context::current()->instance<sand::geoinfo>();
      auto budget    = std::make_unique<station_material_budget>(gi.tracker());
      UFW_INFO("sand::tracker::kalman::engine: material from tracker_info, {} layers in {} stations.",
               budget->layers().size(), budget->extents().size());
      m_material = std::move(budget);
    } else if (material != "none") {
      UFW_ERROR("sand::tracker::kalman::engine: 'material' must be \"none\" or \"tracker_info\", got \"{}\".",
                material);
    }

    auto const b = m_field->at(0., 0., 0.);
    UFW_INFO("sand::tracker::kalman::engine: uniform field ({}, {}, {}) T, drift velocity {} mm/ns, wire velocity {} "
             "mm/ns, TDC resolution {} ns, t0 offset {} ns, max step {} mm, max slope {}, material {}.",
             b[0], b[1], b[2], m_calibration.drift_velocity, m_calibration.wire_velocity, m_calibration.tdc_resolution,
             m_calibration.t0_offset, m_propagator.max_step, m_propagator.max_slope, material);
  }

  engine::~engine() = default;

  std::optional<propagation_result> engine::propagate(const state& in, double z_target, double mass) const {
    if (m_material) {
      return kalman::propagate(in, z_target, mass, *m_field, *m_material, m_propagator);
    }
    return kalman::propagate(in, z_target, mass, *m_field, m_propagator);
  }

  std::optional<propagation_result> engine::propagate(const state& in, double z_target, double mass,
                                                      const material_budget& material) const {
    return kalman::propagate(in, z_target, mass, *m_field, material, m_propagator);
  }

  prediction engine::wire_time(const state& s, const wire_geometry& w, int side) const {
    return kalman::wire_time(s, w, side, m_calibration);
  }

} // namespace sand::tracker::kalman
