#include "gluckstern_smearing.hpp"

#include <caf/caf_wrapper.hpp>

#include <edep_reader/edep_reader.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>

#include <ufw/factory.hpp>

#include <duneanaobj/StandardRecord/SRLorentzVector.h>
#include <duneanaobj/StandardRecord/SRNDBranch.h>
#include <duneanaobj/StandardRecord/SRTrueInteraction.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>
#include <duneanaobj/StandardRecord/SRVector3D.h>

#include <edep_reader/EDEPHit.h>

#include <TDatabasePDG.h>
#include <TParticlePDG.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <random>

namespace sand::common {

  namespace {
    double x0(int z, int a) { return 716.408 * a / (z * (z + 1) * std::log(287. / std::sqrt(z))); }

    double density(sand::root_tgeomanager& tgm) {
      return tgm.navigator()->GetCurrentNode()->GetVolume()->GetMaterial()->GetDensity() * 1.602179e-19 /* to g/cm3 */;
    }

    double path_length(sand::root_tgeomanager& tgm) { return tgm.navigator()->GetStep() * 0.1 /* mm to cm */; }

    double constexpr mm_to_m(double x) { return x / 1000.; }

    double constexpr kMaxRelativePtResolution = 1.0;

    ::caf::SRTrueParticle const& true_particle_from_id(::caf::SRTrueInteraction const& true_ixn,
                                                       ::caf::TrueParticleID const& id) {
      return (id.type == ::caf::TrueParticleID::kPrimary) ? true_ixn.prim[id.part] : true_ixn.sec[id.part];
    }

    ::caf::SRVector3D normalize_to_direction(float px, float py, float pz) {
      float const mag = std::sqrt(px * px + py * py + pz * pz);
      return (mag > 0.f) ? ::caf::SRVector3D{px / mag, py / mag, pz / mag} : ::caf::SRVector3D{0.f, 0.f, 0.f};
    }

    EDEPHitsMap::const_iterator find_tracker_hits(EDEPHitsMap const& hit_map) {
      if (auto it = hit_map.find(sand::subdetector_t::DRIFT); it != hit_map.end()) {
        return it;
      }
      return hit_map.find(sand::subdetector_t::STT);
    }

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

  std::optional<::caf::SRVector3D> smear_momentum_gluckstern(::caf::SRLorentzVector const& true_p,
                                                             GlucksternGeometry const& geom, double sigma_t,
                                                             double sigma_l, double b_field) {
    double const p_transverse = std::hypot(true_p.Y(), true_p.Z());
    double const dip_angle    = std::atan(true_p.X() / p_transverse);
    double const zy_angle     = std::atan2(true_p.Y(), true_p.Z());
    double const p            = true_p.Mag();

    double const pt_res =
        std::hypot(gluckstern_pt_resolution(p_transverse, geom, sigma_t, b_field), mcs_pt_resolution(geom, b_field, p));
    double const dip_res =
        std::hypot(gluckstern_dip_resolution(geom, sigma_l), mcs_angle_resolution(geom.path_len_over_x0_full, p));
    double const zy_res = mcs_angle_resolution(geom.path_len_over_x0_transverse, p);

    if (!std::isfinite(pt_res) || !std::isfinite(dip_res) || !std::isfinite(zy_res)
        || pt_res > kMaxRelativePtResolution) {
      return std::nullopt;
    }

    auto& rng = ufw::context::current()->engine();

    std::normal_distribution<double> pt_dist(1.0, pt_res);
    double pt_factor = pt_dist(rng);
    while (pt_factor <= 0.) {
      pt_factor = pt_dist(rng);
    }
    double const smeared_p_transverse = p_transverse * pt_factor;

    double const smeared_dip_angle = dip_angle + std::normal_distribution<double>(0.0, dip_res)(rng);
    double const smeared_zy_angle  = zy_angle + std::normal_distribution<double>(0.0, zy_res)(rng);

    return ::caf::SRVector3D{static_cast<float>(smeared_p_transverse * std::tan(smeared_dip_angle)),
                             static_cast<float>(smeared_p_transverse * std::sin(smeared_zy_angle)),
                             static_cast<float>(smeared_p_transverse * std::cos(smeared_zy_angle))};
  }

  gluckstern_smearing::gluckstern_smearing()
    : process{{{"in_nd", "sand::caf::nd_reco_branch_wrapper"}, {"in_truth", "sand::caf::truth_branch_wrapper"}},
              {{"out_nd", "sand::caf::nd_reco_branch_wrapper"}}} {}

  void gluckstern_smearing::configure(ufw::config const& cfg) {
    process::configure(cfg);
    m_sigma_t        = mm_to_m(cfg.value("intrinsic_pos_res_t", 0.2));
    m_sigma_l        = mm_to_m(cfg.value("intrinsic_pos_res_l", 2.));
    m_hit_energy_thr = cfg.value("hit_energy_thr", 0.);
    m_b_field        = cfg.value("b_field_magnitude", 0.6);

    UFW_ASSERT(m_b_field > 0., "gluckstern_smearing: b_field_magnitude must be > 0, got {}", m_b_field);
  }

  void gluckstern_smearing::run() {
    auto const& in_nd    = get<sand::caf::nd_reco_branch_wrapper>("in_nd");
    auto const& in_truth = get<sand::caf::truth_branch_wrapper>("in_truth");
    auto& out_nd         = set<sand::caf::nd_reco_branch_wrapper>("out_nd");
    auto& edep           = instance<sand::edep_reader>();
    auto& tgm            = instance<sand::root_tgeomanager>();

    out_nd.sand.nixn = in_nd.sand.nixn;
    out_nd.sand.ixn.reserve(in_nd.sand.ixn.size());

    int n_smeared{};
    int n_out_of_range{};

    for (std::size_t ixn_idx{}, sand_ixn_size{in_nd.sand.ixn.size()}; ixn_idx != sand_ixn_size; ++ixn_idx) {
      auto const& true_ixn = in_truth.nu[ixn_idx];
      auto& out_ixn        = out_nd.sand.ixn.emplace_back(in_nd.sand.ixn[ixn_idx]);

      for (auto& track : out_ixn.tracker.tracks) {
        auto const& true_part = true_particle_from_id(true_ixn, track.truth[0]);

        if (std::abs(true_part.pdg) != 13) {
          continue; // only muons get Gluckstern smearing
        }

        auto trj = edep.GetTrajectory(true_part.G4ID);
        if (trj == edep.end()) {
          continue;
        }

        auto const& hit_map = trj->GetHitMap();
        auto const it       = find_tracker_hits(hit_map);
        if (it == hit_map.end()) {
          continue;
        }

        auto const geom = gluckstern_geometry_from_hits(tgm, it->second, m_hit_energy_thr);
        if (!geom) {
          continue;
        }

        auto const smeared_p = smear_momentum_gluckstern(true_part.p, *geom, m_sigma_t, m_sigma_l, m_b_field);
        if (!smeared_p) {
          ++n_out_of_range; // resolution not physically usable (see kMaxRelativePtResolution); left unsmeared
          continue;
        }

        auto const* pdg_info = TDatabasePDG::Instance()->GetParticle(true_part.pdg);
        float const mass     = pdg_info != nullptr ? static_cast<float>(pdg_info->Mass()) : 0.f;

        track.dir    = normalize_to_direction(smeared_p->x, smeared_p->y, smeared_p->z);
        track.enddir = track.dir;
        track.E      = std::hypot(std::hypot(smeared_p->x, smeared_p->y), std::hypot(smeared_p->z, mass));
        track.Evis   = track.E;

        ++n_smeared;
      }
    }

    UFW_DEBUG("gluckstern_smearing: smeared {} muon tracks, {} left unsmeared (resolution out of range)", n_smeared,
              n_out_of_range);
  }

} // namespace sand::common

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::common::gluckstern_smearing);
