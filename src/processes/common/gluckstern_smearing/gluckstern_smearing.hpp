#ifndef SAND_COMMON_GLUCKSTERN_SMEARING_HPP
#define SAND_COMMON_GLUCKSTERN_SMEARING_HPP

#include <common/version.h>

#include <edep_reader/EDEPHit.h>

#include <ufw/process.hpp>

#include <optional>
#include <vector>

namespace sand {
  class root_tgeomanager;
}

namespace sand::common {

  struct GlucksternGeometry {
    int n_hits;
    double lever_arm;                   // [m], YZ plane
    double path_len_over_x0_full;       // complete 3d path, in X0 units
    double path_len_over_x0_transverse; // YZ projection, in unità di X0
  };

  // At least 2 hits above hit_energy_thr are required; nullopt if the track doesn't have enough.
  [[nodiscard]] std::optional<GlucksternGeometry> gluckstern_geometry_from_hits(
      sand::root_tgeomanager& tgm, std::vector<EDEPHit> const& trk_hits, double hit_energy_thr);

  double gluckstern_pt_resolution(double p_t, GlucksternGeometry const& geometry, double sigma_t, double b_field);

  double mcs_pt_resolution(GlucksternGeometry const& geometry, double b_field, double p);

  double gluckstern_dip_resolution(GlucksternGeometry const& geometry, double sigma_l);

  double mcs_angle_resolution(double len_over_x0, double p);

  class gluckstern_smearing : public ufw::process {
    gluckstern_smearing();
    void configure(ufw::config const& cfg) override;
    void run() override;
  };

} // namespace sand::common

#endif
