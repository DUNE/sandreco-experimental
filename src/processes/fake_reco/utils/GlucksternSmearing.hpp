#pragma once

#include <common/version.h>
#include <ufw/context.hpp>

#include <edep_reader/EDEPTrajectory.h>
#include <edep_reader/EDEPTree.h>

#include <duneanaobj/StandardRecord/SRRecoParticle.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>

#include <root_tgeomanager/root_tgeomanager.hpp>

#include <algorithm>

namespace smearing {

  constexpr double k_edepsim_density_to_g_cm3 = 6.42E18;

  /** @brief Calculates the radiation length (X0) according to the formula from: https://cds.cern.ch/record/1279627/files/PH-EP-Tech-Note-2010-013.pdf
 */
  inline double get_x0(int z, int a) { return 716.408 /*g/cm2*/ * a / (z * (z + 1) * log(287 / sqrt(z))); }

  /// @brief Gets the density of the current material in g/cm³
  inline double get_density_g_cm3(sand::root_tgeomanager& tgm) {
    return tgm.navigator()->GetCurrentNode()->GetVolume()->GetMaterial()->GetDensity() / k_edepsim_density_to_g_cm3;
  }

  /// @brief Calculates the path length through the material in c
  inline double get_path_len_in_cm(sand::root_tgeomanager& tgm) { return tgm.navigator()->GetStep() * 0.1; }

  enum class Mode { full, transverse };

  /// @brief Calculates the ratio between path length and radiation length
  template <Mode M>
  double get_path_len_over_x0(const std::vector<sand::vec_4d>& hit_points);

  /// @brief Calculates the smearing angle due to multiple Coulomb scattering (MCS)
  inline double compute_mcs_angle_smearing(const double path_len_over_x0, const double p) { // p in GeV
    return (13.6e-3 / p * sqrt(path_len_over_x0) * (1 + 0.038 * log(path_len_over_x0)));
  };

  namespace gluckstern {

    struct PathLengthOverX0 {
      double full;
      double transverse;
    };

    struct GlucksternSmearing {
      // FIXME: hardcoded smearing parameters (pass a Config helper struct from the process)
      constexpr double k_single_hit_sigma  = 200e-6;    // [m]
      constexpr double k_b_field_magnitude = 0.6;       // [T]
      constexpr double k_hit_energy_thr    = 250e-6;    // [MeV]
      constexpr double k_light_velocity    = 299792458; // [m/s]
      constexpr double k_e_charge          = 1.602e-19; // [C]
      constexpr double k_MeV_to_J          = 1.602e-13;

      GlucksternSmearing(double sigma, double b, const std::vector<EDEPHit>& trk_hits);
      GlucksternSmearing(const std::vector<EDEPHit>& trk_hits);

      /// @brief Applies smearing to the four-momentum vector
      caf::SRVector3D apply_smearing(const caf::SRLorentzVector& true_p) const;

      /// @brief Checks if the smearing was correctly initialized (i.e. > 2 hit points)
      bool IsValid() const { return m_smearing_enabled; }

     private:
      int m_n_pts;                         // number of trajectory hits in the tracker
      double m_lever_arm;                  // lever arm from the trajectory hits in the tracker [m] (zy plane)
      PathLengthOverX0 m_path_len_over_x0; // cumulative l/x0 over the trajectory path in the tracker
      bool m_smearing_enabled = false;

      static ufw::context::random_engine& m_random_engine() { return ufw::context::current()->engine(); };

      /// @brief Calculates the detector resolution contribution to Gluckstern smearing
      double compute_measurement_smearing(const double p_transverse) const {
        return ((k_single_hit_sigma * p_transverse) / (0.3 * k_b_field_magnitude * m_lever_arm * m_lever_arm))
             * std::sqrt(720.0 / (m_n_pts + 4));
      }

      /// @brief Calculates the multiple Coulomb scattering contribution to Gluckstern smearing
      double compute_mcs_measurement_smearing(const double path_len_over_x0_tr) const {
        return (
            ((19.2 * k_MeV_to_J) / (std::sqrt(2) * m_lever_arm * k_e_charge * k_b_field_magnitude * k_light_velocity))
            * std::sqrt(path_len_over_x0_tr));
      }
    };

  } // namespace gluckstern
} // namespace smearing