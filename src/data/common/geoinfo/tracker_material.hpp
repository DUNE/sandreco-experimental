#pragma once

#include <geoinfo/tracker_info.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

class TGeoMaterial;

namespace sand {

  /**
   * @brief Material properties used to describe the tracker stations (@ref geoinfo::tracker_info::material_layer).
   *
   * Radiation length, density and composition come from the ROOT geometry. The mean excitation energy and the
   * Sternheimer density-effect parameters are not stored in the geometry: they come from a small table keyed by
   * material name (@ref find_material_constants) or, for the mean excitation energy of materials not in the table,
   * from the Bragg additivity rule over the elements (@ref bragg_mean_excitation).
   */
  namespace tracker_material {

    /// One element of a material, with its mass fraction.
    struct element_fraction {
      int z;           ///< Atomic number
      double a;        ///< Atomic mass [g/mol]
      double fraction; ///< Mass fraction w (the fractions of a material sum to 1)
    };

    /// Tabulated constants of a material that the ROOT geometry does not carry.
    struct material_constants {
      std::string_view name;                                                    ///< Name in the geometry
      double mean_excitation;                                                   ///< I [MeV]
      std::optional<geoinfo::tracker_info::sternheimer_parameters> sternheimer; ///< Density effect
      std::string_view source;                                                  ///< Where the values come from
    };

    /// Density of 1 g/cm3 expressed in the Geant4 internal units used by the edep-sim geometries.
    inline constexpr double geant4_g_per_cm3 = 6.24150907e18;

    double element_mean_excitation(int z);

    double z_over_a(const std::vector<element_fraction>& elements);

    double bragg_mean_excitation(const std::vector<element_fraction>& elements);

    const material_constants* find_material_constants(std::string_view name);

    std::vector<element_fraction> elements(const TGeoMaterial& material);

    double density(const TGeoMaterial& material);

    geoinfo::tracker_info::material_layer properties(const TGeoMaterial& material);

  } // namespace tracker_material

} // namespace sand
