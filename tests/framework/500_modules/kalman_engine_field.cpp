#include <kalman/engine.hpp>
#include <kalman/field.hpp>
#include <kalman/propagator.hpp>

#include <ufw/config.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace sand::test {

  /**
   * Check the field of the Kalman engine at configured points, and that a propagation inside the tracker volume is the
   * same as with the uniform field of the magnet.
   *
   * Configuration: "points", a list of { "where" : <label>, "position" : [x, y, z] (mm), "field" : [Bx, By, Bz] (T) }.
   */
  class kalman_engine_field : public ufw::process {
   public:
    kalman_engine_field() : process({}, {}) {}
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    struct point {
      std::string where;
      std::array<double, 3> position;
      tracker::kalman::field_vector field;
    };
    std::vector<point> m_points;
  };

  void kalman_engine_field::configure(const ufw::config& cfg) {
    process::configure(cfg);
    for (auto const& p : cfg.at("points")) {
      m_points.push_back({p.at("where").get<std::string>(), p.at("position").get<std::array<double, 3>>(),
                          p.at("field").get<tracker::kalman::field_vector>()});
    }
  }

  void kalman_engine_field::run() {
    auto const& kf = instance<tracker::kalman::engine>();
    for (auto const& p : m_points) {
      auto const b = kf.field().at(p.position[0], p.position[1], p.position[2]);
      UFW_INFO("Field in {} at ({}, {}, {}) mm: ({}, {}, {}) T", p.where, p.position[0], p.position[1], p.position[2],
               b[0], b[1], b[2]);
      for (int i = 0; i < 3; ++i) {
        UFW_ASSERT(std::abs(b[i] - p.field[i]) < 1e-9, "Field in {}: component {} is {} T, expected {} T", p.where, i,
                   b[i], p.field[i]);
      }
    }

    // In the tracker the geometry field is uniform: same propagation as with the uniform field of the magnet
    using namespace tracker::kalman;
    state s;
    s.z          = 22910.;
    s.par[X]     = 100.;
    s.par[Y]     = -2184.73;
    s.par[TX]    = 0.1;
    s.par[TY]    = -0.2;
    s.par[QOP]   = -1. / 800.;
    auto const a = kf.propagate(s, 24910., 105.6583755);
    auto const u = propagate(s, 24910., 105.6583755, uniform_field({0.6, 0., 0.}), kf.propagator_settings());
    UFW_ASSERT(a && u, "Propagation in the tracker failed");
    for (unsigned int i = 0; i < s_dim; ++i) {
      UFW_ASSERT(a->out.par[i] == u->out.par[i], "Parameter {} differs from the uniform-field propagation: {} vs {}", i,
                 a->out.par[i], u->out.par[i]);
    }
    UFW_INFO("Propagation in the tracker identical to the uniform 0.6 T field.");
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::kalman_engine_field)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::kalman_engine_field)
