#include "gluckstern_smearing.hpp"

#include <root_tgeomanager/root_tgeomanager.hpp>

#include <algorithm>
#include <cmath>

namespace sand::common {

  namespace {
    double x0(int z, int a) { return 716.408 * a / (z * (z + 1) * std::log(287. / std::sqrt(z))); }

    double density(sand::root_tgeomanager& tgm) {
      return tgm.navigator()->GetCurrentNode()->GetVolume()->GetMaterial()->GetDensity() * 1.602179e-19 /* to g/cm3 */;
    }

    double path_length(sand::root_tgeomanager& tgm) { return tgm.navigator()->GetStep() * 0.1 /* mm to cm */; }

    double constexpr mm_to_m(double x) { return x / 1000.; }

    double walk_segment(sand::root_tgeomanager::navigator_ptr const& nav, sand::root_tgeomanager& tgm,
                        sand::pos_3d const& current, sand::pos_3d const& next, sand::dir_3d const& dir) {
      nav->set_track(current, dir);
      auto pos = current;
      double acc{};

      while (pos.Z() < next.Z() && std::abs(next.Z() - pos.Z()) > 1e-5) {
        double const step_max = std::min(10., (next - pos).R());
        nav->FindNextBoundary(step_max);

        auto const* material = nav->GetCurrentNode()->GetVolume()->GetMaterial();
        auto const atomic_nr = static_cast<int>(material->GetZ());
        auto const mass_nr   = static_cast<int>(material->GetA());

        acc += path_length(tgm) * density(tgm) / x0(atomic_nr, mass_nr);

        nav->Step(true, true);
        pos = nav->get_point();
      }

      return acc;
    }

    struct PathLengthOverX0 {
      double full;
      double transverse;
    };

    PathLengthOverX0 path_len_over_x0(sand::root_tgeomanager& tgm, std::vector<sand::vec_4d> const& hit_pts) {
      auto nav              = tgm.navigator();
      auto const last_hit_z = static_cast<sand::pos_3d>(hit_pts.back().Vect()).Z();

      PathLengthOverX0 result{0., 0.};

      for (auto it = hit_pts.begin(); it != hit_pts.end() - 1; ++it) {
        auto const current = static_cast<sand::pos_3d>(it->Vect());
        if (current.Z() >= last_hit_z) {
          continue;
        }

        auto const next           = static_cast<sand::pos_3d>(std::next(it)->Vect());
        auto const dir            = (next - current).Unit();
        sand::dir_3d const dir_tr = {0., dir.Y(), dir.Z()};

        result.full += walk_segment(nav, tgm, current, next, dir);
        result.transverse += walk_segment(nav, tgm, current, next, dir_tr);
      }

      return result;
    }
  } // namespace

  std::optional<GlucksternGeometry> gluckstern_geometry_from_hits(sand::root_tgeomanager& tgm,
                                                                  std::vector<EDEPHit> const& trk_hits,
                                                                  double hit_energy_thr) {
    std::vector<sand::vec_4d> hit_pts;
    hit_pts.reserve(trk_hits.size());
    for (auto const& hit : trk_hits) {
      if (hit.GetEnergyDeposit() > hit_energy_thr) {
        hit_pts.push_back(0.5 * (hit.GetStart() + hit.GetStop()));
      }
    }

    if (hit_pts.size() < 2) {
      return std::nullopt;
    }

    auto const delta     = hit_pts.back() - hit_pts.front();
    auto const lever_arm = mm_to_m(std::hypot(delta.Y(), delta.Z()));
    auto const path      = path_len_over_x0(tgm, hit_pts);

    return GlucksternGeometry{static_cast<int>(hit_pts.size()), lever_arm, path.full, path.transverse};
  }

  double gluckstern_pt_resolution(double p_t, GlucksternGeometry const& geometry, double sigma_t, double b_field) {
    auto const l2 = geometry.lever_arm * geometry.lever_arm;
    auto const n  = geometry.n_hits;
    return ((sigma_t * p_t) / (0.3 * b_field * l2)) * std::sqrt(720. / (n + 4));
  }

  double gluckstern_dip_resolution(GlucksternGeometry const& geometry, double sigma_l) {
    auto const l = geometry.lever_arm;
    auto const n = geometry.n_hits;
    return (sigma_l / l) * std::sqrt((12. * (n - 1)) / (n * (n + 1)));
  }

  double mcs_angle_resolution(double len_over_x0, double p) {
    double constexpr highland_factor     = 0.0136; // [GeV], p expected in GeV
    double constexpr highland_log_factor = 0.038;

    return (highland_factor / p) * std::sqrt(len_over_x0) * (1. + highland_log_factor * std::log(len_over_x0));
  }

  double mcs_pt_resolution(GlucksternGeometry const& geom, double b_field, double p) {
    return mcs_angle_resolution(geom.path_len_over_x0_full, p) / (0.3 * b_field * geom.lever_arm);
  }

} // namespace sand::common
