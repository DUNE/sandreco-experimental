#ifndef SANDRECO_FAKE_RECO_UTILS_HPP
#define SANDRECO_FAKE_RECO_UTILS_HPP

#include <duneanaobj/StandardRecord/SRVector3D.h>

#include <cmath>

namespace sand::fake_reco {

  /// Average nucleon mass in GeV
  constexpr float kNucleonMass_GeV = 0.939f;

  /// GENIE "bindino" PDG code - fictitious particle representing nuclear binding energy
  /// https://github.com/DUNE/ND_CAFMaker/blob/972f11bc5b69ea1f595e14ed16e09162f512011e/src/truth/FillTruth.cxx#L223
  constexpr int kBindinoPdg = 2000000101;

  /// Check if PDG code corresponds to a lepton (e, mu, tau or their neutrinos)
  /// PDG codes 11-16 are e-, nu_e, mu-, nu_mu, tau-, nu_tau
  inline bool is_lepton_pdg(int pdg) {
    const int abs_pdg = std::abs(pdg);
    return (abs_pdg >= 11 && abs_pdg <= 16);
  }

  /// Check if PDG code corresponds to a charged lepton (e, mu, tau)
  inline bool is_charged_lepton_pdg(int pdg) {
    const int abs_pdg = std::abs(pdg);
    return (abs_pdg == 11 || abs_pdg == 13 || abs_pdg == 15);
  }

  /// Normalize momentum components to a unit direction vector
  /// Returns zero vector if magnitude is zero
  inline ::caf::SRVector3D normalize_to_direction(float px, float py, float pz) {
    const float mag = std::sqrt(px * px + py * py + pz * pz);
    if (mag > 0.f) {
      return {px / mag, py / mag, pz / mag};
    }
    return {0.f, 0.f, 0.f};
  }

} // namespace sand::fake_reco

#endif // SANDRECO_FAKE_RECO_UTILS_HPP
