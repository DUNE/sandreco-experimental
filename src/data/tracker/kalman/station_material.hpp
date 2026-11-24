#pragma once

#include <kalman/material.hpp>
#include <kalman/state.hpp>

#include <geoinfo/tracker_info.hpp>

#include <cstddef>
#include <vector>

/**
 * @file station_material.hpp
 * @brief Material budget of the tracker stations, built from the material layers of sand::geoinfo::tracker_info.
 *
 * See docs/kalman_filter.md, section "Material of the tracker stations".
 */

namespace sand::tracker::kalman {

  /**
   * @brief Rescale Sternheimer parameters to another density of the same material.
   * @param d     Parameters valid at the table density rho_table
   * @param ratio r = rho / rho_table, the new density over the table density (positive)
   * @return The parameters at rho: C -> C - ln r, x0 -> x0 - 0.5 log10 r, x1 -> x1 - 0.5 log10 r; a, k and delta0
   *         unchanged (Sternheimer, Berger and Seltzer, At. Data Nucl. Data Tables 30 (1984) 261). Parameters that
   *         are not enabled (density_effect::enabled() false) are returned unchanged.
   *
   * The plasma energy scales as sqrt(rho), so C = 2 ln(I / hbar omega_p) + 1 decreases by ln r. The whole delta(X)
   * curve moves to lower X = log10(beta gamma) by 0.5 log10 r: delta_rho(X) = delta_table(X + 0.5 log10 r).
   */
  density_effect rescale_density_effect(const density_effect& d, double ratio);

  /**
   * @brief Density effect parameters of a geoinfo material at the density of the geometry.
   * @param p       Parameters from geoinfo, valid at p.reference_density
   * @param density Density of the material in the geometry [g/cm3]
   * @return The parameters rescaled with rescale_density_effect(), r = density / p.reference_density.
   */
  density_effect to_density_effect(const geoinfo::tracker_info::sternheimer_parameters& p, double density);

  /**
   * @brief Kalman layer equivalent to a geoinfo material layer.
   * @param m The geoinfo layer
   * @return A layer centred at (z_min + z_max) / 2 with thickness z_max - z_min, the same X0, density, <Z/A> and I,
   *         and the Sternheimer parameters rescaled to the density of the layer (no density effect if geoinfo has
   *         none for the material). The transverse extent is left empty: station_material_budget sets it.
   */
  layer to_layer(const geoinfo::tracker_info::material_layer& m);

  /**
   * @brief Material of the tracker stations, from the layers of sand::geoinfo::tracker_info.
   *
   * At construction every layer of every station is converted with to_layer() and given the transverse extent of
   * its station: the rectangle spanned in x and y by the four corners of the station (top_north, top_south,
   * bottom_south, bottom_north). The layers of all stations are kept in one list sorted in z.
   *
   * crossed_layers() returns all the layers in the interval of material_budget::crossed_layers(), ordered along the
   * propagation, each with its extent. Whether the track crosses a station is decided by
   * sand::tracker::kalman::propagate(), at the position propagated in the field to the plane of each layer.
   *
   * Inside the extent of a station the layers do not depend on x and y: they are those sampled by geoinfo along one
   * line per station (see docs/geoinfo_tracker_materials.md).
   */
  class station_material_budget : public material_budget {
   public:
    /// A layer with the index of its station.
    struct station_layer {
      layer material;      ///< The layer, as given by to_layer(), with the extent of its station
      std::size_t station; ///< Index of the station in tracker_info::stations()
    };

    /**
     * @brief Convert the material of all stations of the tracker.
     * @param tracker The tracker description; it is not referenced after construction.
     *
     * Stops the job (UFW_ERROR) if a layer has a non-positive thickness, radiation length or density, or Sternheimer
     * parameters with a non-positive reference density.
     */
    explicit station_material_budget(const geoinfo::tracker_info& tracker);

    /// See material_budget::crossed_layers(). Only s.z and s.direction are used.
    std::vector<layer> crossed_layers(const state& s, double z_target) const override;

    /// All layers of all stations, sorted by increasing z.
    const std::vector<station_layer>& layers() const { return m_layers; }

    /// Transverse extent of each station, indexed as tracker_info::stations().
    const std::vector<transverse_extent>& extents() const { return m_extents; }

   private:
    std::vector<station_layer> m_layers;      ///< Sorted by increasing layer centre
    std::vector<transverse_extent> m_extents; ///< One per station
  };

} // namespace sand::tracker::kalman
