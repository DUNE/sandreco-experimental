#include <geoinfo/field_info.hpp>
#include <geoinfo/geoinfo.hpp>

#include <root_tgeomanager/root_tgeomanager.hpp>

#include <ufw/config.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace sand::test {

  /**
   * Check the magnetic field of sand::geoinfo at configured points, with geoinfo::field_at and with a probe kept
   * across the events of the job, moving the shared navigator between the lookups.
   *
   * Configuration: "points", a list of { "where" : <label>, "position" : [x, y, z] (mm), "field" : [Bx, By, Bz] (T) }.
   */
  class geoinfo_field : public ufw::process {
   public:
    geoinfo_field() : process({}, {}) {}
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    struct point {
      std::string where;
      pos_3d position;
      dir_3d field;
    };
    std::vector<point> m_points;
    sand::geoinfo::field_probe m_probe; ///< Bound at the first event, kept across events
  };

  void geoinfo_field::configure(const ufw::config& cfg) {
    process::configure(cfg);
    for (auto const& p : cfg.at("points")) {
      auto const x = p.at("position").get<std::array<double, 3>>();
      auto const b = p.at("field").get<std::array<double, 3>>();
      m_points.push_back({p.at("where").get<std::string>(), pos_3d(x[0], x[1], x[2]), dir_3d(b[0], b[1], b[2])});
    }
  }

  void geoinfo_field::run() {
    auto const& gi = instance<sand::geoinfo>();
    UFW_ASSERT(gi.has_field(), "sand::geoinfo has no field");
    if (!m_probe) {
      m_probe = gi.make_field_probe();
    }
    auto nav = instance<sand::root_tgeomanager>().navigator();
    for (auto const& p : m_points) {
      auto const a = gi.field_at(p.position);
      nav->FindNode(0., 0., 0.); // other code moves the shared navigator
      auto const b = m_probe.field_at(p.position);
      UFW_INFO("Field in {} at ({}, {}, {}) mm: ({}, {}, {}) T", p.where, p.position.X(), p.position.Y(),
               p.position.Z(), a.X(), a.Y(), a.Z());
      UFW_ASSERT(a == b, "Field in {}: field_at and the probe differ", p.where);
      UFW_ASSERT(std::abs(a.X() - p.field.X()) < 1e-9 && std::abs(a.Y() - p.field.Y()) < 1e-9
                     && std::abs(a.Z() - p.field.Z()) < 1e-9,
                 "Field in {}: ({}, {}, {}) T, expected ({}, {}, {}) T", p.where, a.X(), a.Y(), a.Z(), p.field.X(),
                 p.field.Y(), p.field.Z());
    }
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::geoinfo_field)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::geoinfo_field)
