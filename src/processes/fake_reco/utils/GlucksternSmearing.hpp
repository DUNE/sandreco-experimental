#pragma once

#include <common/version.h>
#include <ufw/context.hpp>

#include <edep_reader/EDEPTrajectory.h>
#include <edep_reader/EDEPTree.h>

#include <duneanaobj/StandardRecord/SRRecoParticle.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>

#include <root_tgeomanager/root_tgeomanager.hpp>

#include <algorithm>

namespace sand {

  namespace {
    [[nodiscard]] constexpr inline float mev_to_gev(float E) { return E / 1000.0f; }

    [[nodiscard]] constexpr inline float gev_to_mev(float E) { return E * 1000.0f; }

    [[nodiscard]] constexpr inline float mm_to_m(float x) { return x / 1000.0f; }
  } // namespace

  namespace constants {
    constexpr double x0_factor                = 716.408; // [g/cm2]
    constexpr double log_argument             = 287;
    constexpr double mm_to_cm                 = 0.1;
    constexpr double highland_factor          = 13.6; // [MeV]
    constexpr double highland_log_factor      = 0.038;
    constexpr double light_velocity           = TMath::C();  // [m/s]
    constexpr double e_charge                 = TMath::Qe(); // [C]
    constexpr double MeV_to_J                 = TMath::Qe() * 1e6;
    constexpr double edepsim_density_to_g_cm3 = 6.2415e18;
    constexpr double gluckstern_factor        = 0.3; // GeV/(T·m)
    constexpr double gluckstern_n_pts_factor  = 720.;
    constexpr double gluckstern_angle_factor  = 12.;

  } // namespace constants

  namespace k = constants;

  /** @brief Calculates the radiation length (X0) according to the formula from:
   * https://cds.cern.ch/record/1279627/files/PH-EP-Tech-Note-2010-013.pdf
   */
  inline double get_x0(int z, int a) { return k::x0_factor * a / (z * (z + 1) * log(k::log_argument / sqrt(z))); }

  /// @brief Gets the density of the current material in g/cm³
  inline double get_density_g_cm3(sand::root_tgeomanager& tgm) {
    return tgm.navigator()->GetCurrentNode()->GetVolume()->GetMaterial()->GetDensity() / k::edepsim_density_to_g_cm3;
  }

  /// @brief Calculates the path length through the material in cm
  inline double get_path_len_in_cm(sand::root_tgeomanager& tgm) { return tgm.navigator()->GetStep() * k::mm_to_cm; }

  enum class Mode { full, transverse };

  /// @brief Calculates the ratio between path length and radiation length
  template <Mode M>
  double get_path_len_over_x0(const std::vector<sand::vec_4d>& hit_points);

  /// @brief Calculates the smearing angle due to multiple Coulomb scattering (MCS)
  inline double compute_mcs_angle_smearing(const double path_len_over_x0, const double p) { // p in GeV
    const auto highland_factor_gev = mev_to_gev(k::highland_factor);
    return (highland_factor_gev / p * sqrt(path_len_over_x0) * (1 + k::highland_log_factor * log(path_len_over_x0)));
  };

  struct PathLengthOverX0 {
    double full;
    double transverse;
  };

  struct GlucksternSmearing {
    const double intrinsic_pos_res_t = 200e-3; // single hit y resolution in mm
    const double intrinsic_pos_res_l = 2;      // single hit x resolution in mm
    const double b_field_magnitude   = 0.6;    // magnetic field magnitude in Tesla

    GlucksternSmearing(double sigma_y, double sigma_x, double b, const std::vector<EDEPHit>& trk_hits);
    GlucksternSmearing(const std::vector<EDEPHit>& trk_hits, const double hit_energy_thr);

    /// @brief Applies smearing to the four-momentum vector
    caf::SRVector3D apply_smearing(const caf::SRLorentzVector& true_p, const double intrinsic_pos_res_t,
                                   const double intrinsic_pos_res_l, const double b_field_magnitude) const;

    /// @brief Checks if the smearing was correctly initialized (i.e. > 2 hit points)
    bool IsValid() const { return m_smearing_enabled; }

    int GetNHits() const { return m_n_pts; }

   private:
    int m_n_pts;                                 // number of trajectory hits in the tracker
    double m_lever_arm;                          // lever arm from the trajectory hits in the tracker [m] (zy plane)
    mutable PathLengthOverX0 m_path_len_over_x0; // cumulative l/x0 over the trajectory path in the tracker
    bool m_smearing_enabled = false;
    std::vector<sand::vec_4d> m_hit_pts_above_thr;

    static ufw::context::random_engine& m_random_engine() { return ufw::context::current()->engine(); };

    /// @brief Calculates the detector resolution contribution to Gluckstern smearing
    double compute_measurement_smearing(const double p_transverse, const double intrinsic_pos_res_t,
                                        const double b_field_magnitude) const {
      return ((intrinsic_pos_res_t * p_transverse)
              / (k::gluckstern_factor * b_field_magnitude * m_lever_arm * m_lever_arm))
           * std::sqrt(k::gluckstern_n_pts_factor / (m_n_pts + 4));
    }

    /// @brief Calculates the multiple Coulomb scattering contribution to Gluckstern smearing
    double compute_mcs_measurement_smearing(const double path_len_over_x0_tr, const double b_field_magnitude) const {
      return (((k::highland_factor * k::MeV_to_J) / (m_lever_arm * k::e_charge * b_field_magnitude * k::light_velocity))
              * std::sqrt(path_len_over_x0_tr));
    }

    /// @brief Calculates the detector resolution contribution to the dip angle measurement
    double compute_angle_measurement_smearing(const double intrinsic_pos_res_l) const {
      return ((intrinsic_pos_res_l / m_lever_arm)
              * std::sqrt(k::gluckstern_angle_factor * (m_n_pts - 1) / m_n_pts * (m_n_pts + 1)));
    }
  };

} // namespace sand