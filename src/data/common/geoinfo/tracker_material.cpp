/**
 * @file
 * @brief Material properties of the tracker stations (@ref sand::tracker_material): element and material tables,
 *        <Z/A>, Bragg additivity rule and conversion from TGeoMaterial.
 */

#include <tracker_material.hpp>

#include <TGeoMaterial.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace sand::tracker_material {

  namespace {

    constexpr double eV = 1e-6; // in MeV

    /**
     * Mean excitation energies of the elements found in the SAND geometries, plus the noble gases that may be used
     * in the chambers (He, Ne, Xe).
     * Source: ICRU Report 37 (1984), as tabulated in Geant4 10.6.3, G4NistManager::GetMeanIonisationEnergy(Z).
     * Carbon is 81 eV here (the Geant4 element value) while the graphite entry of the material table has 78 eV.
     */
    constexpr std::array<std::pair<int, double>, 28> s_element_mean_excitation{{
        {1, 19.2 * eV},   {2, 41.8 * eV},   {5, 76.0 * eV},   {6, 81.0 * eV},   {7, 82.0 * eV},   {8, 95.0 * eV},
        {9, 115.0 * eV},  {10, 137.0 * eV}, {11, 149.0 * eV}, {12, 156.0 * eV}, {13, 166.0 * eV}, {14, 173.0 * eV},
        {15, 173.0 * eV}, {16, 180.0 * eV}, {18, 188.0 * eV}, {19, 190.0 * eV}, {20, 191.0 * eV}, {22, 233.0 * eV},
        {24, 257.0 * eV}, {25, 272.0 * eV}, {26, 286.0 * eV}, {28, 311.0 * eV}, {29, 322.0 * eV}, {30, 330.0 * eV},
        {54, 482.0 * eV}, {74, 727.0 * eV}, {79, 790.0 * eV}, {82, 823.0 * eV},
    }};

    constexpr std::string_view s_table_source =
        "I: ICRU Report 37 (1984), Geant4 10.6.3 G4NistMaterialBuilder; density effect: Sternheimer, Berger, Seltzer, "
        "At. Data Nucl. Data Tables 30 (1984) 261, Geant4 10.6.3 G4DensityEffectData";

    /**
     * Mean excitation energy and Sternheimer parameters of the tracker materials, keyed by their name in the
     * geometry. I is the ICRU Report 37 value of the Geant4 10.6.3 NIST material; the Sternheimer parameters are the
     * entry of Geant4 10.6.3 G4DensityEffectData for that material (Sternheimer, Berger and Seltzer 1984), unchanged.
     * The reference density is the one implied by the plasma energy of that entry, hbar omega_p =
     * 28.816 eV sqrt(rho <Z/A>), which is the density the parameters were computed at.
     * The chamber gases (driftGas, stGas_*) are Ar/CO2 mixtures with no tabulated entry: they get I from the Bragg
     * rule and no density effect.
     */
    const std::array<material_constants, 3> s_material_constants{{
        // Graphite: G4_GRAPHITE, G4DensityEffectData index 276. The Geant4 material has 2.210 g/cm3, but the
        // plasma energy of the entry (30.652 eV) corresponds to 2.265 g/cm3.
        {"Graphite", 78.0 * eV,
         geoinfo::tracker_info::sternheimer_parameters{0.26142, 2.8697, -0.0178, 2.3415, 2.8680, 0.12, 2.265},
         s_table_source},
        // Polypropylene (C3H6)n: G4_POLYPROPYLENE, index 222, density 0.900 g/cm3 (the plasma energy, 20.457 eV,
        // gives 0.884 g/cm3 with the <Z/A> of CH2: the tabulated composition differs slightly).
        {"C3H6", 56.5 * eV,
         geoinfo::tracker_info::sternheimer_parameters{0.15045, 3.2855, 0.1534, 2.4822, 3.1252, 0.0, 0.900},
         s_table_source},
        // Mylar: G4_MYLAR, index 219, 1.400 g/cm3 (plasma energy 24.595 eV).
        {"Mylar", 78.7 * eV,
         geoinfo::tracker_info::sternheimer_parameters{0.12679, 3.3076, 0.1562, 2.6507, 3.3262, 0.0, 1.400},
         s_table_source},
    }};

  } // namespace

  /**
   * @brief Mean excitation energy of an element.
   * @param z Atomic number.
   * @return I [MeV].
   * @throws ufw::exception if the element is not in the table.
   */
  double element_mean_excitation(int z) {
    auto it = std::find_if(s_element_mean_excitation.begin(), s_element_mean_excitation.end(),
                           [z](const auto& e) { return e.first == z; });
    if (it == s_element_mean_excitation.end()) {
      UFW_ERROR("No mean excitation energy for element Z = {}: add it to the table in tracker_material.cpp.", z);
    }
    return it->second;
  }

  /**
   * @brief Mean <Z/A> of a material, sum_i w_i Z_i / A_i over its elements.
   * @param elements Elements with mass fractions w_i.
   * @return <Z/A> [mol/g].
   */
  double z_over_a(const std::vector<element_fraction>& elements) {
    double sum = 0.0;
    for (const auto& e : elements) {
      sum += e.fraction * e.z / e.a;
    }
    return sum;
  }

  /**
   * @brief Mean excitation energy of a compound from the Bragg additivity rule,
   *        ln I = sum_i w_i (Z_i/A_i) ln I_i / sum_i w_i (Z_i/A_i).
   * @param elements Elements with mass fractions w_i.
   * @return I [MeV].
   * @throws ufw::exception if an element is not in the element table or the list is empty.
   */
  double bragg_mean_excitation(const std::vector<element_fraction>& elements) {
    double num = 0.0;
    double den = 0.0;
    for (const auto& e : elements) {
      double weight = e.fraction * e.z / e.a;
      num += weight * std::log(element_mean_excitation(e.z));
      den += weight;
    }
    if (!(den > 0.0)) {
      UFW_ERROR("Cannot apply the Bragg rule to a material without elements.");
    }
    return std::exp(num / den);
  }

  /**
   * @brief Looks up the tabulated constants of a material.
   * @param name Name of the material in the geometry (exact, case-sensitive match).
   * @return The table entry, or nullptr if the material is not in the table.
   */
  const material_constants* find_material_constants(std::string_view name) {
    auto it = std::find_if(s_material_constants.begin(), s_material_constants.end(),
                           [name](const material_constants& m) { return m.name == name; });
    return it == s_material_constants.end() ? nullptr : &*it;
  }

  /**
   * @brief Elements of a ROOT material with their mass fractions.
   *
   * Isotopes are listed by the edep-sim geometries as separate entries with the same Z; they are kept as they are,
   * since <Z/A> and the Bragg rule are sums over entries.
   *
   * @param material The ROOT material or mixture.
   * @return One entry per component.
   */
  std::vector<element_fraction> elements(const TGeoMaterial& material) {
    std::vector<element_fraction> ret;
    if (material.IsMixture()) {
      const auto& mix = static_cast<const TGeoMixture&>(material);
      for (int i = 0; i != mix.GetNelements(); ++i) {
        ret.push_back({int(std::lround(mix.GetZmixt()[i])), mix.GetAmixt()[i], mix.GetWmixt()[i]});
      }
    } else {
      ret.push_back({int(std::lround(material.GetZ())), material.GetA(), 1.0});
    }
    return ret;
  }

  /**
   * @brief Density of a ROOT material in g/cm3.
   *
   * The edep-sim geometries store densities in Geant4 internal units (see @ref geant4_g_per_cm3), while lengths are
   * in mm.
   *
   * @param material The ROOT material.
   * @return Density [g/cm3].
   */
  double density(const TGeoMaterial& material) { return material.GetDensity() / geant4_g_per_cm3; }

  /**
   * @brief Material properties of a layer made of a ROOT material (the z extent is left at zero).
   *
   * Radiation length and density come from the geometry; <Z/A> from the composition; I and the Sternheimer
   * parameters from the material table if the material is there, otherwise I from the Bragg rule and no density
   * effect.
   *
   * @param material The ROOT material.
   * @return A layer with all properties but z_min and z_max set.
   */
  geoinfo::tracker_info::material_layer properties(const TGeoMaterial& material) {
    geoinfo::tracker_info::material_layer layer{};
    auto elems             = elements(material);
    layer.material         = material.GetName();
    layer.radiation_length = material.GetRadLen();
    layer.density          = density(material);
    layer.z_over_a         = z_over_a(elems);
    if (const auto* tab = find_material_constants(layer.material)) {
      layer.mean_excitation = tab->mean_excitation;
      layer.sternheimer     = tab->sternheimer;
    } else {
      layer.mean_excitation = bragg_mean_excitation(elems);
    }
    return layer;
  }

} // namespace sand::tracker_material
