#ifndef SAND_COMMON_GLUCKSTERN_SMEARING_HPP
#define SAND_COMMON_GLUCKSTERN_SMEARING_HPP

#include <common/version.h>

#include <ufw/process.hpp>

#include <optional>
#include <vector>

namespace sand {
  class root_tgeomanager;
}

namespace caf {
  class SRLorentzVector;
  class SRVector3D;
} // namespace caf

class EDEPHit;

namespace sand::common {

  struct GlucksternGeometry {
    int n_hits;
    double lever_arm;                   // [m], YZ plane
    double path_len_over_x0_full;       // complete 3d path, in X0 units
    double path_len_over_x0_transverse; // YZ projection, in unità di X0
  };

  [[nodiscard]] std::optional<GlucksternGeometry> gluckstern_geometry_from_hits(sand::root_tgeomanager& tgm,
                                                                                std::vector<EDEPHit> const& trk_hits,
                                                                                double hit_energy_thr);

  double gluckstern_pt_resolution(double p_t, GlucksternGeometry const& geometry, double sigma_t, double b_field);

  double mcs_pt_resolution(GlucksternGeometry const& geometry, double b_field, double p);

  double gluckstern_dip_resolution(GlucksternGeometry const& geometry, double sigma_l);

  double mcs_angle_resolution(double len_over_x0, double p);

  /// \brief Smears the momentum via the Gluckstern+MCS formulas, or `std::nullopt` if the
  /// resulting resolution is not physically usable (see `gluckstern_smearing.cpp` for why).
  [[nodiscard]] std::optional<::caf::SRVector3D> smear_momentum_gluckstern(::caf::SRLorentzVector const& true_p,
                                                                           GlucksternGeometry const& geom,
                                                                           double sigma_t, double sigma_l,
                                                                           double b_field);

  class gluckstern_smearing : public ufw::process {
    double m_sigma_t{};        // [m]
    double m_sigma_l{};        // [m]
    double m_hit_energy_thr{}; // [MeV]
    double m_b_field{};        // [T]

   public:
    gluckstern_smearing();
    void configure(ufw::config const& cfg) override;
    void run() override;
  };

} // namespace sand::common

UFW_REGISTER_PROCESS(sand::common::gluckstern_smearing);

#endif
