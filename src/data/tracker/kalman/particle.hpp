#pragma once

/**
 * @file particle.hpp
 * @brief Particle properties used for the mass hypothesis of the track fit.
 *
 * The library takes the mass hypothesis as an argument (propagate(), engine::propagate()). Code that needs the mass of
 * a particle species gets it from the particle table by PDG code with mass_from_pdg(), instead of writing the value.
 */

namespace sand::tracker::kalman {

  /**
   * @brief Mass of a particle species [MeV], from ROOT's TDatabasePDG.
   *
   * The table gives the mass in GeV; it is converted with CLHEP::GeV. Each code is looked up once and cached; the
   * function is thread safe.
   *
   * @param pdg PDG code (the sign is irrelevant for the mass, but is passed to the table as given)
   * @return Mass [MeV]
   * @throws std::invalid_argument if the code is not in the table
   */
  double mass_from_pdg(int pdg);

} // namespace sand::tracker::kalman
