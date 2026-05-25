#pragma once
#include "common/version.h"
#include <ufw/context.hpp>
#include <edep_reader/EDEPTrajectory.h>
#include <edep_reader/EDEPTree.h>
#include <duneanaobj/StandardRecord/SRInteraction.h>
#include <duneanaobj/StandardRecord/SRRecoParticle.h>
#include <duneanaobj/StandardRecord/SRShower.h>
#include <duneanaobj/StandardRecord/SRTrack.h>
#include <duneanaobj/StandardRecord/SRTrueInteraction.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>
#include <root_tgeomanager/root_tgeomanager.hpp>

#include <cmath>
#include <random>

namespace smearer {
  class EnergySmearer {
   private:
    static ufw::context::random_engine& kRng() {return ufw::context::current()->engine();};
    const float k_E_stochastic = 0.057f; // sigma = 5.7% sqrt(E) in gev
    const float k_E_noise      = 0.01f;  // sigma = 1%
    const float k_E_constant   = 0.01f;  // sigma = 1% E

   public:
    inline EnergySmearer();
    void E_smearing(::caf::SRRecoParticle& part);
    float sum_quad(float E);
  };

  namespace Gluckstern {
    const double k_edepsim_density_to_g_cm3 = 6.42E18;
    const double k_single_hit_sigma  = 200e-6;    // [m]
    const double k_b_field_magnitude = 0.6;       // [T]
    const double k_hit_energy_thr    = 250e-6;    // [MeV]
    const double k_light_velocity    = 299792458; // [m/s]
    const double k_e_charge          = 1.602e-19; // [C]
    const double k_MeV_to_J          = 1.602e-13;


    inline double compute_x0(int nr_atom, int nr_mass) {
      return 716.408 * nr_mass / (nr_atom * (nr_atom + 1) * log(287 / sqrt(nr_atom)));
    } // g cm^-2
    inline double get_density_g_cm3(sand::root_tgeomanager& tgm) {
      return tgm.navigator()->GetCurrentNode()->GetVolume()->GetMaterial()->GetDensity() / k_edepsim_density_to_g_cm3;
    }
    inline double get_L_in_cm(sand::root_tgeomanager& tgm) { return tgm.navigator()->GetStep() * 0.1; }

    enum class Mode {full, transverse};
    template <Mode M>
    double get_L_over_x0(const std::vector<sand::vec_4d> hit_points);
    inline double compute_mcs_angle_smearing(const double L_over_x0, const double p) {
        return ( 13.6e-3/p * sqrt(L_over_x0) * (1 + 0.038 * log(L_over_x0)) );
      };
    class Gluckstern_smearer{
      private:
      static ufw::context::random_engine& kRng() {return ufw::context::current()->engine();};
      int k_num_hits;
      double k_lever_arm;
      double k_L_over_x0_full;
      double k_L_over_x0_transverse;

      public:
      Gluckstern_smearer();

      inline double compute_measurement_smearing(const double p_transverse) {
        return (k_single_hit_sigma * p_transverse) / (0.3 * k_b_field_magnitude * k_lever_arm)
             * std::sqrt(720.0 / (k_num_hits + 4));
      }
      inline double compute_mcs_measurement_smearing(const double L_over_x0_tr) {
        return ( ( (19.2 * k_MeV_to_J) / (std::sqrt(2)*k_lever_arm*k_e_charge*k_b_field_magnitude*k_light_velocity)) 
              * std::sqrt(L_over_x0_tr) );
      }
      void p_smearing(::caf::SRRecoParticle& part);

    };
  } // namespace Gluckstern

} // namespace smearer