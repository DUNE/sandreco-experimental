#pragma once

#include <ufw/context.hpp>

#include <edep_reader/EDEPTrajectory.h>
#include <edep_reader/EDEPTree.h>

#include <duneanaobj/StandardRecord/SRTrueParticle.h>
#include <duneanaobj/StandardRecord/SRRecoParticle.h>

#include <algorithm>

namespace smearing{
    namespace gluckstern{


        struct GlucksternSmearing{
            // FIXME: hardcoded smearing parameters (pass a Config helper struct from the process)
            const double k_single_hit_sigma  = 200e-3; // [mm]
            const double k_b_field_magnitude = 0.6; // [T]
            const double k_hit_energy_thr    = 250e-6; // [MeV]


            GlucksternSmearing(double sigma, double b, const std::vector<EDEPHit>& trk_hits);
            GlucksternSmearing(const std::vector<EDEPHit>& trk_hits);

            caf::SRVector3D apply_smearing(const caf::SRLorentzVector& true_p) const;


        private:
            int m_n_pts; // number of trajectory hits in the tracker
            double m_lever_arm; // lever arm from the trajectory hits in the tracker
            double m_path_len_over_x0; // cumulative l/X0 over the trajectory path in the tracker

            static ufw::context::random_engine& m_random_engine() { return ufw::context::current()->engine(); };

            double compute_measurement_smearing(const double p_transverse) const {
                return (k_single_hit_sigma*p_transverse)/(0.3*k_b_field_magnitude*m_lever_arm*m_lever_arm)*std::sqrt(720.0/(m_n_pts+4));
            }

        };


    }
}