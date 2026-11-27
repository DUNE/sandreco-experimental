#pragma once

/**
 * @file particle.hpp
 * @brief Particle hypothesis of the track fit: mass and absolute charge.
 *
 * The library takes the particle hypothesis as an argument (propagate(), engine::propagate(), momentum()). Code that
 * needs the hypothesis of a particle species gets it from the particle table by PDG code with hypothesis_from_pdg(),
 * instead of writing the values. See docs/kalman_filter.md, section "2.1 State x and covariance C", paragraph
 * "Particle hypothesis".
 */

namespace sand::tracker::kalman {

  /**
   * @brief Particle hypothesis of a track: mass and absolute charge.
   *
   * The state carries q/p, with q the signed charge in units of e (state.hpp). The curvature depends on q/p only, so it
   * is valid for any charge; the hypothesis is needed where p and m enter separately: the time of flight
   * (1/beta = sqrt(1 + (m/|q|)^2 (q/p)^2)) and the momentum p = |q| / |q/p| (momentum()).
   *
   * No default values, as for the other structs of the library: code that builds it directly sets both members, or
   * gets it from hypothesis_from_pdg().
   */
  struct particle_hypothesis {
    double mass;       ///< Mass [MeV]; zero or positive
    double abs_charge; ///< Absolute charge |q| [units of e]; positive (1 for e, mu, pi, K, p; 2 for an alpha)
  };

  /**
   * @brief Particle hypothesis of a species, from ROOT's TDatabasePDG.
   *
   * The table gives the mass in GeV, converted with CLHEP::GeV, and the charge in units of |e|/3, converted to units
   * of e (|Charge()| / 3). Each code is looked up once and cached; the function is thread safe.
   *
   * The default table of ROOT has no nuclei: a code such as 1000020040 (alpha) is found only if the job has added it
   * to TDatabasePDG (TDatabasePDG::AddParticle(), or a table read with ReadPDGTable()). Otherwise build the
   * particle_hypothesis directly.
   *
   * @param pdg PDG code (the sign is irrelevant for mass and |q|, but is passed to the table as given)
   * @return Mass [MeV] and |q| [e]
   * @throws std::invalid_argument if the code is not in the table, or if the particle is neutral (the fit needs a
   *         charged track)
   */
  particle_hypothesis hypothesis_from_pdg(int pdg);

} // namespace sand::tracker::kalman
