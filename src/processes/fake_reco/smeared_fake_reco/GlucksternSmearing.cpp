#include "GlucksternSmearing.hpp"

namespace smearing{
    namespace gluckstern{

        
        GlucksternSmearing::GlucksternSmearing(double sigma, double b, const std::vector<EDEPHit>& trk_hits): k_single_hit_sigma(sigma), k_b_field_magnitude(b){}

        GlucksternSmearing::GlucksternSmearing(const std::vector<EDEPHit>& trk_hits){
            // filter the hits below k_hit_energy_thr and fill the mean coordinates
            std::vector<sand::vec_4d> hit_pts_above_thr;
            hit_pts_above_thr.reserve(trk_hits.size());

            for(const auto& hit : trk_hits){
                if(hit.GetEnergyDeposit() > k_hit_energy_thr){
                    hit_pts_above_thr.push_back(0.5*(hit.GetStart()+hit.GetStop()));
                }
            }
            
            if (hit_pts_above_thr.size()<2) { // require at least 2 points
              UFW_ERROR("Trajectory has <2 hits above energy threshold.");
            } else {
              m_n_pts = hit_pts_above_thr.size(); // set the number of points for Gluckstern smearing
              // lever arm in the bending plane (YZ) for Gluckstern formula
              const auto hits_delta = hit_pts_above_thr.back() - hit_pts_above_thr.front();
              m_lever_arm = std::sqrt(hits_delta.Y() * hits_delta.Y() +
                                      hits_delta.Z() * hits_delta.Z());
            }
            
            // then compute path_len/X0
        }

        caf::SRVector3D GlucksternSmearing::apply_smearing(const caf::SRLorentzVector& true_p) const {

            const double p_transverse = std::sqrt(true_p.Y()*true_p.Y()+true_p.Z()*true_p.Z());
            const auto measure_pt_res = compute_measurement_smearing(p_transverse);
            
            // random extraction of the smearing factor (force a positive Pt)
            std::normal_distribution<double> relative_pt_error(1.0, measure_pt_res);
            double ran = relative_pt_error(m_random_engine());
            while(ran<=0){
                ran = relative_pt_error(m_random_engine());
            }
            const double smeared_p_transverse = p_transverse * ran;

            auto true_unit_vec = true_p.Vect().Unit();

            return caf::SRVector3D{true_p.X(), true_unit_vec.Y()*smeared_p_transverse,true_unit_vec.Z()*smeared_p_transverse};
            
        }

    }
}