#include "caf_filler.hpp"

#include "GlucksternSmearing.hpp"

namespace sand {

  ::caf::SRRecoParticle CAFFiller<::caf::SRRecoParticle>::from_true_with_mu_smearing(const ::caf::SRTrueParticle& true_part,
                                                                    const ::caf::TrueParticleID& id, const EDEPTrajectory& true_part_trj) {
    // start by filling all fields from truth
    auto reco = CAFFiller<::caf::SRRecoParticle>::from_true(true_part, id);
    // return from_true for all particle except muons (temporarily)
    if(std::abs(true_part.pdg)!=13) { return reco; }

    // for muons in the drift tracker (temporarily) smear according to the Gluckstern formula
    const auto& hit_map = true_part_trj.GetHitMap();
    const auto& it = hit_map.find(component::DRIFT);
    if(it == hit_map.end()) { return reco; }
    const auto& hit_vec = it->second;

    const auto gluckstern_helper = smearing::gluckstern::GlucksternSmearing{hit_vec};

    // apply the measurement resolution smearing
    reco.p = gluckstern_helper.apply_smearing(true_part.p);

    // /* Estimate the number of bending plane measurements as
    // the number of hits with energy deposit > 250e-6 MeV (from ND-SAND-Fastreco)
    // Ideally, vertical (enough) planes should be excluded */
    // const int n_pts = std::count_if(hit_vec.begin(), hit_vec.end(), [](const EDEPHit& hit){return hit.GetEnergyDeposit() > 250e-6;});

    // // constant pars, TODO: find a better placement
    // const double single_hit_sigma = 200e-3; // [mm]
    // const double b_field_magnitude = 0.6; // [T]
    // // lever-arm from truth values
    // const double delta_y = (true_part.end_pos-true_part.start_pos).Y();
    // const double delta_z = (true_part.end_pos-true_part.start_pos).Z();
    // const double lever_arm = std::sqrt(delta_y*delta_y+delta_z*delta_z);
    // const double p_transverse = std::sqrt(true_part.p.Y()*true_part.p.Y()+true_part.p.Z()*true_part.p.Z());
    
    // // measurement Gluckstern smearing factor
    // const double measure_pt_res = (single_hit_sigma*p_transverse)/(0.3*b_field_magnitude*lever_arm)*std::sqrt(720.0/(n_pts+4));
    // // 

    // // random extraction of the smearing factor (force a positive Pt)
    // std::normal_distribution<double> relative_pt_error(1.0, measure_pt_res);
    // double ran = relative_pt_error(random_engine());
    // while(ran<=0){
    //   ran = relative_pt_error(random_engine());
    // }
    // const double smeared_p_transverse = p_transverse * ran;
    // // reset the magnitude of the reco Pt
    // auto true_unit_vec = true_part.p.Vect().Unit();
    // reco.p.SetY(true_unit_vec.Y()*smeared_p_transverse);
    // reco.p.SetZ(true_unit_vec.Z()*smeared_p_transverse);
    return reco;
  }

}