#include <kalman/engine.hpp>
#include <kalman/geometry_field.hpp>

#include <ufw/config.hpp>
#include <ufw/context.hpp>

#include <initializer_list>
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
    check_keys(cfg, "top level", {"field", "calibration", "propagator"});

    if (!cfg.contains("field")) {
      UFW_ERROR("sand::tracker::kalman::engine: missing required key 'field'.");
    }
    std::string field_description;
    auto const& f = cfg.at("field");
    if (f.is_array()) {
      if (f.size() != 3 || !f[0].is_number() || !f[1].is_number() || !f[2].is_number()) {
        UFW_ERROR("sand::tracker::kalman::engine: 'field' must be an array of three numbers [T], got {}.", f.dump());
      }
      auto const b      = f.get<field_vector>();
      m_field           = std::make_unique<uniform_field>(b);
      field_description = fmt::format("uniform field ({}, {}, {}) T", b[0], b[1], b[2]);
    } else if (f.is_object()) {
      check_keys(f, "field", {"source", "file"});
      if (!f.contains("source") || f.at("source") != "gdml") {
        UFW_ERROR("sand::tracker::kalman::engine: 'field.source' must be \"gdml\", got {}.",
                  f.contains("source") ? f.at("source").dump() : std::string("nothing"));
      }
      if (!f.contains("file")) {
        UFW_ERROR("sand::tracker::kalman::engine: missing required key 'field.file'.");
      }
      if (!f.at("file").is_string()) {
        UFW_ERROR("sand::tracker::kalman::engine: 'field.file' must be a path, got {}.", f.at("file").dump());
      }
      // Relative paths are resolved against ufw-basepath, as for the geometry of root_tgeomanager
      auto const file   = ufw::config(f).path_at("file");
      auto const tags   = read_gdml_field_tags(file);
      auto& geo         = ufw::context::current()->instance<sand::root_tgeomanager>();
      m_field           = std::make_unique<volume_field>(geo, tags);
      field_description = fmt::format("field of the geometry volumes, tags from '{}'", file.string());
    } else {
      UFW_ERROR("sand::tracker::kalman::engine: 'field' must be an array [Bx, By, Bz] in T or an object "
                "{{ \"source\" : \"gdml\", \"file\" : <path> }}, got {}.",
                f.dump());
    }

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

    UFW_INFO("sand::tracker::kalman::engine: {}, drift velocity {} mm/ns, wire velocity {} "
             "mm/ns, TDC resolution {} ns, t0 offset {} ns, max step {} mm, max slope {}.",
             field_description, m_calibration.drift_velocity, m_calibration.wire_velocity, m_calibration.tdc_resolution,
             m_calibration.t0_offset, m_propagator.max_step, m_propagator.max_slope);
  }

  engine::~engine() = default;

  std::optional<propagation_result> engine::propagate(const state& in, double z_target, double mass) const {
    return kalman::propagate(in, z_target, mass, *m_field, m_propagator);
  }

  prediction engine::wire_time(const state& s, const wire_geometry& w, int side) const {
    return kalman::wire_time(s, w, side, m_calibration);
  }

} // namespace sand::tracker::kalman
