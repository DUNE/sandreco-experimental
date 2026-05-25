#include "common/version.h"
#include "smearer.hpp"

namespace smearer {
  float EnergySmearer::sum_quad(float E) {
    auto sigma_stochastic = k_E_stochastic * std::sqrt(E);
    auto sigma_noise      = k_E_noise;
    auto sigma_constant   = k_E_constant * E;
    return static_cast<float>(std::sqrt(pow(sigma_stochastic, 2) + pow(sigma_noise, 2) + pow(sigma_constant, 2)));
  }
  void EnergySmearer::E_smearing(::caf::SRRecoParticle& part) {
    const auto E_stddev = sum_quad(part.E);
    std::normal_distribution<double> gaus(0, E_stddev);
    const auto sigma = gaus(kRng());
    part.E           = std::max(0.0f, static_cast<float>(part.E + sigma));
    return;
  }
  namespace Gluckstern {
    template<Mode M>
    double get_L_over_x0(const std::vector<sand::vec_4d> hit_points){
      auto& tgm = ufw::context::current()->instance<sand::root_tgeomanager>();
      auto nav = tgm.navigator();

      auto start = static_cast<sand::pos_3d>(hit_points.front().Vect());
      auto finish = static_cast<sand::pos_3d>(hit_points.back().Vect());
      auto initial_volume = nav->find_node(start);
      double L_over_x0= 0.;
      for (auto it=hit_points.begin(); it != hit_points.end()-1; ++it){
        auto current_hit_pos = static_cast<sand::pos_3d>(it->Vect());
        if (current_hit_pos.Z()<finish.Z()){
          auto next_hit_pos = static_cast<sand::pos_3d>(std::next(it)->Vect());
          auto direction = (next_hit_pos - current_hit_pos).unit();
          auto direction_transverse = direction;
          direction_transverse.SetX(0);
          direction_transverse.unit();
          if constexpr (M == Mode::transverse){
            nav->set_track(current_hit_pos, direction_transverse);
          } else {
            nav->set_track(current_hit_pos, direction);
          }
          sand::pos_3d last_pos = current_hit_pos;
          while(last_pos.Z()<next_hit_pos.Z() && std::abs(next_hit_pos.Z()-last_pos.Z()) > 1E-5){
            double distance_to_next_hit = (next_hit_pos-last_pos).R();
            double step_max = std::min(10.,distance_to_next_hit);
            nav->FindNextBoundary(step_max);
            auto atomic_num = static_cast<int>(nav->GetCurrentNode()->GetVolume()->GetMaterial()->GetZ());
            auto mass_num = static_cast<int>(nav->GetCurrentNode()->GetVolume()->GetMaterial()->GetA());
            auto material_name = static_cast<std::string>(nav->GetCurrentNode()->GetVolume()->GetMaterial()->GetName());
            auto rad_length = compute_x0(atomic_num,mass_num);
            auto density = get_density_g_cm3(tgm);
            auto path_length = get_L_in_cm(tgm);
            L_over_x0 += path_length * density / rad_length;
            nav->Step(true,true);
            last_pos = nav->get_point();
          }
        } 
      }
      return L_over_x0;
    }
    Gluckstern_smearer::Gluckstern_smearer(const std::vector<EDEPHit>& hits) {
        std::vector<sand::vec_4d> hits_above_thr;
        hits_above_thr.reserve(hits.size());

        for(const auto& hit : hits){
          if(hit.GetEnergyDeposit() > k_hit_energy_thr){
            hits_above_thr.push_back(0.5*(hit.GetStart()+hit.GetStop()));
          }
        } 
            
            if (hits_above_thr.size()<2) { 
              UFW_ERROR("Trajectory has <2 hits above energy threshold.");
            } else {
              k_num_hits = hits_above_thr.size(); 
              const auto hits_delta = hits_above_thr.back() - hits_above_thr.front();
              k_lever_arm = std::hypot(hits_delta.Y(), hits_delta.Z());
              k_lever_arm /= 1E3; // mm -> m
            }
            
            
            k_L_over_x0_full = get_L_over_x0<Mode::full>(hits_above_thr);
            k_L_over_x0_transverse = get_L_over_x0<Mode::transverse>(hits_above_thr);
        }
      void Gluckstern_smearer::p_smearing(::caf::SRRecoParticle& part){
        if (std::abs(part.pdg != 13)){return;} //only smear muon p
        const auto true_p = part.p;
        const float p_transverse = std::hypot(true_p.Y(),true_p.Z());
        const float dip_angle = std::atan(true_p.X()/p_transverse);
        const float bending_angle = std::atan2(true_p.Y(),true_p.Z());
        
        const auto pt_measure_res = compute_measurement_smearing(p_transverse / 1000.0f );
        const auto pt_mcs_res = compute_mcs_measurement_smearing(k_L_over_x0_full);
        const auto pt_res = std::hypot(pt_measure_res,pt_mcs_res);
        const auto dip_angle_mcs_res = compute_mcs_angle_smearing(k_L_over_x0_full, true_p.Mag() / 1000.0f ); 
        const auto bending_angle_mcs_res = compute_mcs_angle_smearing(k_L_over_x0_transverse, p_transverse / 1000.0f );

        std::normal_distribution<float> pt_gaus (0, pt_res);
        const auto pt_sigma = pt_gaus(kRng());
        const float smeared_pt = static_cast<float>(std::max(0.0f, p_transverse + pt_sigma));
        std::normal_distribution<float> dip_angle_gaus (0,dip_angle_mcs_res);
        const float dip_angle_sigma = dip_angle_gaus(kRng());
        const float smeared_dip_angle = dip_angle + dip_angle_sigma;
        std::normal_distribution<float> bending_angle_gaus (0,bending_angle_mcs_res);
        const float  bending_angle_sigma = bending_angle_gaus(kRng());
        const float smeared_bending_angle = bending_angle + bending_angle_sigma;

        part.p.SetX(p_transverse * std::tan(smeared_dip_angle));
        part.p.SetY(smeared_pt * std::sin(smeared_bending_angle));
        part.p.SetZ(smeared_pt * std::cos(smeared_bending_angle));
      }

  } // namespace Gluckstern

} // namespace smearer