#pragma once

#include <ufw/context.hpp>

#include <edep_reader/EDEPTrajectory.h>
#include <edep_reader/EDEPTree.h>

#include <duneanaobj/StandardRecord/SRRecoParticle.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>

#include <root_tgeomanager/root_tgeomanager.hpp>

#include <algorithm>

namespace smearing {

      const double k_edepsim_density_to_g_cm3 = 6.42E18;

      // formula taken from here: https://cds.cern.ch/record/1279627/files/PH-EP-Tech-Note-2010-013.pdf
      inline double get_x0(int z, int a) {
        return 716.408 /*g/cm2*/ * a / (z * (z+1) * log(287/sqrt(z)));
      }

      inline double get_density_g_cm3(sand::root_tgeomanager& tgm) {
        return tgm.navigator()->GetCurrentNode()->GetVolume()->GetMaterial()->GetDensity() / k_edepsim_density_to_g_cm3; 
      }

      inline double get_path_len_in_cm(sand::root_tgeomanager& tgm) {
        return tgm.navigator()->GetStep() * 0.1;
      }
      
      enum class Mode {full, transverse };

      template<Mode M>
      double get_path_len_over_x0(const std::vector<sand::vec_4d> hit_points);

      inline double compute_mcs_angle_smearing(const double path_len_over_x0, const double p) {
        return ( 13.6e-3/p * sqrt(path_len_over_x0) * (1 + 0.038 * log(path_len_over_x0)) );
      };

  namespace gluckstern {

    struct PathLengthOverX0{
      double full;
      double transverse;
    };

    struct GlucksternSmearing {
      // FIXME: hardcoded smearing parameters (pass a Config helper struct from the process)
      const double k_single_hit_sigma  = 200e-6;    // [m]
      const double k_b_field_magnitude = 0.6;       // [T]
      const double k_hit_energy_thr    = 250e-6;    // [MeV]
      const double k_light_velocity    = 299792458; // [m/s]
      const double k_e_charge          = 1.602e-19; // [C]
      const double k_MeV_to_J          = 1.602e-13;

      GlucksternSmearing(double sigma, double b, const std::vector<EDEPHit>& trk_hits);
      GlucksternSmearing(const std::vector<EDEPHit>& trk_hits);

      caf::SRVector3D apply_smearing(const caf::SRLorentzVector& true_p) const;

     private:
      int m_n_pts;               // number of trajectory hits in the tracker
      double m_lever_arm;        // lever arm from the trajectory hits in the tracker [m] (zy plane)
      PathLengthOverX0 m_path_len_over_x0; // cumulative l/x0 over the trajectory path in the tracker

      static ufw::context::random_engine& m_random_engine() { return ufw::context::current()->engine(); };

      // detector res contribution to Gluckstern smearing: using transverse lever_arm
      double compute_measurement_smearing(const double p_transverse) const {
        return (k_single_hit_sigma * p_transverse) / (0.3 * k_b_field_magnitude * m_lever_arm)
             * std::sqrt(720.0 / (m_n_pts + 4));
      }

      // MCS contribution to GLuckstern smearing: using transverse lever_arm
      double compute_mcs_measurement_smearing(const double path_len_over_x0_tr) const {
        return ( ( (19.2 * k_MeV_to_J) / (std::sqrt(2)*m_lever_arm*k_e_charge*k_b_field_magnitude*k_light_velocity)) 
              * std::sqrt(path_len_over_x0_tr) );
      }
    };

  } // namespace gluckstern
} // namespace smearing