#include "GlucksternSmearing.hpp"
#include <fstream>

namespace smearing {

  [[nodiscard]] constexpr float mev_to_gev(float E) { return E / 1000.0f; }

  [[nodiscard]] constexpr float gev_to_mev(float E) { return E * 1000.0f; }

  template <Mode M>
  double get_path_len_over_x0(const std::vector<sand::vec_4d>& hit_points) {
    // here hit_points are taken as the medium point of the original HitSegment

    auto& tgm = ufw::context::current()->instance<sand::root_tgeomanager>();
    auto nav  = tgm.navigator();

    auto first_hit_point = static_cast<sand::pos_3d>(hit_points.front().Vect());
    auto last_hit_point  = static_cast<sand::pos_3d>(hit_points.back().Vect());

    double path_len_over_x0 = 0.;

    for (auto it = hit_points.begin(); it != hit_points.end() - 1; ++it) {
      auto current_hit_pos = static_cast<sand::pos_3d>(it->Vect());

      // check if the current hit is not the last one
      if (current_hit_pos.Z() < last_hit_point.Z()) {
        auto next_hit_pos = static_cast<sand::pos_3d>(std::next(it)->Vect());

        auto current_dir = (next_hit_pos - current_hit_pos).unit();

        if constexpr (M == Mode::transverse) {
          nav->set_track(current_hit_pos, {0., current_dir.Y(), current_dir.Z()});
        } else {
          nav->set_track(current_hit_pos, current_dir);
        }

        sand::pos_3d last_pos = current_hit_pos;

        // let the navigator go along the current direction until the next hit is found
        // or the navigator is very close to the next hit (to avoid stalled situations in case of the end of a track)
        while (last_pos.Z() < next_hit_pos.Z() && std::abs(next_hit_pos.Z() - last_pos.Z()) > 1E-5) {
          // needed otherwise FindNextBoundary may not see the next hit, trying to do the whole step up to the next
          // boundary. But if between the last pos and the boundary there is another hit, we have that last_pos >
          // next_hit, so it exits the while, and the navigator assumes the new hit position which has a lower Z wrt to
          // the last pos. In this way, at most the navigator makes a step to reach the next hit
          double distance_to_next_hit = (next_hit_pos - last_pos).R();
          double step_max             = std::min(10., distance_to_next_hit);

          nav->FindNextBoundary(step_max);

          auto atomic_nr     = static_cast<int>(nav->GetCurrentNode()->GetVolume()->GetMaterial()->GetZ());
          auto mass_nr       = static_cast<int>(nav->GetCurrentNode()->GetVolume()->GetMaterial()->GetA());
          auto material_name = static_cast<std::string>(nav->GetCurrentNode()->GetVolume()->GetMaterial()->GetName());

          auto rad_length  = get_x0(atomic_nr, mass_nr); // g/cm2
          auto density     = get_density_g_cm3(tgm);
          auto path_length = get_path_len_in_cm(tgm);

          path_len_over_x0 += path_length * density / rad_length;

          nav->Step(true, true);
          last_pos = nav->get_point();
        }
      }
    }

    UFW_INFO("All hits processed");

    return path_len_over_x0;
  }

  namespace gluckstern {

    GlucksternSmearing::GlucksternSmearing(double sigma, double b, const std::vector<EDEPHit>& trk_hits)
      : k_single_hit_sigma(sigma), k_b_field_magnitude(b) {}

    GlucksternSmearing::GlucksternSmearing(const std::vector<EDEPHit>& trk_hits) {
      // filter the hits below k_hit_energy_thr and fill the mean coordinates
      std::vector<sand::vec_4d> hit_pts_above_thr;
      hit_pts_above_thr.reserve(trk_hits.size());

      for (const auto& hit : trk_hits) {
        if (hit.GetEnergyDeposit() > k_hit_energy_thr) {
          hit_pts_above_thr.push_back(0.5 * (hit.GetStart() + hit.GetStop()));
        }
      }

      if (hit_pts_above_thr.size() < 2) { // require at least 2 points
        UFW_INFO("Trajectory has <2 hits above energy threshold.");
        m_smearing_enabled = false;
      } else {
        m_n_pts = hit_pts_above_thr.size(); // set the number of points for Gluckstern smearing
        // lever arm in the bending plane (YZ) for Gluckstern formula
        const auto hits_delta = hit_pts_above_thr.back() - hit_pts_above_thr.front();
        m_lever_arm           = std::hypot(hits_delta.Y(), hits_delta.Z());
        m_lever_arm /= 1E3; // mm -> m

        // then compute path_len/x0
        m_path_len_over_x0.full = get_path_len_over_x0<Mode::full>(hit_pts_above_thr);
        m_path_len_over_x0.transverse = get_path_len_over_x0<Mode::transverse>(hit_pts_above_thr);
        m_smearing_enabled = true;
      }
    }

    caf::SRVector3D GlucksternSmearing::apply_smearing(const caf::SRLorentzVector& true_p) const {
      const float p_transverse = std::hypot(true_p.Y(), true_p.Z());
      const auto dip_angle     = std::atan(true_p.X() / p_transverse);
      const auto zy_angle      = std::atan2(true_p.Y(), true_p.Z());

      // convert to gev
      const auto p_transverse_gev = mev_to_gev(p_transverse);
      const auto true_p_gev       = mev_to_gev(true_p.Mag());

      // pt resolution (det + MCS)
      const auto measure_pt_res = compute_measurement_smearing(p_transverse_gev);
      const auto mcs_pt_res     = compute_mcs_measurement_smearing(m_path_len_over_x0.full);
      const auto pt_res         = std::hypot(measure_pt_res, mcs_pt_res);

      // MCS angle smearing: for zy used transverse path and transverse p
      const auto mcs_dip_angle_res = compute_mcs_angle_smearing(m_path_len_over_x0.full, true_p_gev);
      const auto mcs_zy_angle_res  = compute_mcs_angle_smearing(m_path_len_over_x0.transverse, true_p_gev);

      // random extraction of the smearing factor (force a positive Pt)
      std::normal_distribution<double> relative_pt_error(1.0, pt_res);
      double ran_pt = relative_pt_error(m_random_engine());
      while (ran_pt <= 0) {
        ran_pt = relative_pt_error(m_random_engine());
      }
      const float smeared_p_transverse_gev = p_transverse_gev * ran_pt;

      // random extraction of the smearing factor
      std::normal_distribution<double> abs_dip_angle_error(0.0, mcs_dip_angle_res);
      double ran_dip_angle          = abs_dip_angle_error(m_random_engine());
      const float smeared_dip_angle = dip_angle + ran_dip_angle;

      // random extraction of the smearing factor
      std::normal_distribution<double> abs_zy_angle_error(0.0, mcs_zy_angle_res);
      double ran_zy_angle          = abs_zy_angle_error(m_random_engine());
      const float smeared_zy_angle = zy_angle + ran_zy_angle;

      return caf::SRVector3D{gev_to_mev(p_transverse_gev) * std::tan(smeared_dip_angle),
                             gev_to_mev(smeared_p_transverse_gev) * std::sin(smeared_zy_angle),
                             gev_to_mev(smeared_p_transverse_gev) * std::cos(smeared_zy_angle)};
    }
  } // namespace gluckstern
  template double get_path_len_over_x0<Mode::full>(const std::vector<sand::vec_4d>&);
  template double get_path_len_over_x0<Mode::transverse>(const std::vector<sand::vec_4d>&);
} // namespace smearing