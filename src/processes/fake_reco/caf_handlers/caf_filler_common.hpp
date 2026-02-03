#ifndef SANDRECO_FAKE_RECO_CAF_FILLER_COMMON_HPP
#define SANDRECO_FAKE_RECO_CAF_FILLER_COMMON_HPP

/// @file caf_filler_common.hpp
/// @brief Utilities for CAF fillers: PDG classification, vector math, truth matching

#include <ufw/utils.hpp>

#include <duneanaobj/StandardRecord/SREnums.h>
#include <duneanaobj/StandardRecord/SRTrueInteraction.h>
#include <duneanaobj/StandardRecord/SRVector3D.h>

#include <TDatabasePDG.h>
#include <TParticlePDG.h>

#include <cmath>
#include <string_view>

namespace sand {

  inline constexpr float kNucleonMass_GeV = 0.939f;
  inline constexpr int kBindinoPdg        = 2000000101;

  [[nodiscard]] inline const TParticlePDG* get_pdg_info(int pdg) { return TDatabasePDG::Instance()->GetParticle(pdg); }

  [[nodiscard]] inline short charge_from_pdg(int pdg) {
    if (const auto* p = get_pdg_info(pdg)) {
      const double q = p->Charge() / 3.0;
      if (q > 0.5) {
        return +1;
      }
      if (q < -0.5) {
        return -1;
      }
    }
    return 0;
  }

  [[nodiscard]] inline bool is_lepton_pdg(int pdg) {
    if (const auto* p = get_pdg_info(pdg)) {
      const char* pclass = p->ParticleClass();
      return pclass && std::string_view{pclass} == "Lepton";
    }
    return false;
  }

  [[nodiscard]] inline bool is_charged_lepton_pdg(int pdg) { return is_lepton_pdg(pdg) && charge_from_pdg(pdg) != 0; }

  /// Track-like: mu, pi+/-, K+/-, p
  [[nodiscard]] inline bool is_track_like(int pdg) {
    const int abs_pdg = std::abs(pdg);
    return abs_pdg == 13 || abs_pdg == 211 || abs_pdg == 321 || abs_pdg == 2212;
  }

  /// Shower-like: e+/-, gamma, pi0
  [[nodiscard]] inline bool is_shower_like(int pdg) {
    const int abs_pdg = std::abs(pdg);
    return abs_pdg == 11 || abs_pdg == 22 || abs_pdg == 111;
  }

  [[nodiscard]] inline ::caf::SRVector3D normalize_to_direction(float px, float py, float pz) {
    const float mag = std::sqrt(px * px + py * py + pz * pz);
    return (mag > 0.f) ? ::caf::SRVector3D{px / mag, py / mag, pz / mag} : ::caf::SRVector3D{0.f, 0.f, 0.f};
  }

  [[nodiscard]] inline float distance(const ::caf::SRVector3D& a, const ::caf::SRVector3D& b) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float dz = b.z - a.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
  }

  [[nodiscard]] inline ::caf::TrueParticleID make_primary_id(long int interaction_id, int particle_index) {
    return {static_cast<int>(interaction_id), ::caf::TrueParticleID::kPrimary, particle_index};
  }

  [[nodiscard]] inline ::caf::TrueParticleID make_secondary_id(long int interaction_id, int particle_index) {
    return {static_cast<int>(interaction_id), ::caf::TrueParticleID::kSecondary, particle_index};
  }

  [[nodiscard]] inline ::caf::TrueParticleID make_prefsi_id(long int interaction_id, int particle_index) {
    return {static_cast<int>(interaction_id), ::caf::TrueParticleID::kPrimaryBeforeFSI, particle_index};
  }

  template <typename T>
  void add_truth_match(T& obj, const ::caf::TrueParticleID& id) {
    obj.truth.push_back(id);
    obj.truthOverlap.push_back(1.0f);
  }

  inline void increment_particle_counter(::caf::SRTrueInteraction& ixn, int pdg) {
    switch (pdg) {
    case 2212:
      ++ixn.nproton;
      break;
    case 2112:
      ++ixn.nneutron;
      break;
    case 211:
      ++ixn.npip;
      break;
    case -211:
      ++ixn.npim;
      break;
    case 111:
      ++ixn.npi0;
      break;
    default:
      UFW_DEBUG("Particle PDG {} not counted", pdg);
      break;
    }
  }

} // namespace sand

#endif // SANDRECO_FAKE_RECO_CAF_FILLER_COMMON_HPP
