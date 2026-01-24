#ifndef SANDRECO_FAKE_RECO_TRACKS_SHOWERS_HPP
#define SANDRECO_FAKE_RECO_TRACKS_SHOWERS_HPP

#include "utils.hpp"

#include <duneanaobj/StandardRecord/SRTrack.h>
#include <duneanaobj/StandardRecord/SRShower.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>

#include <cmath>

namespace sand::fake_reco {

  /// Returns true if the particle should be reconstructed as a track (charged, non-EM)
  inline bool is_track_like(int pdg) {
    const int abs_pdg = std::abs(pdg);
    switch (abs_pdg) {
    case 13:   // muon
    case 211:  // charged pion
    case 321:  // charged kaon
    case 2212: // proton
      return true;
    default:
      return false;
    }
  }

  /// Returns true if the particle should be reconstructed as a shower (EM particles)
  inline bool is_shower_like(int pdg) {
    const int abs_pdg = std::abs(pdg);
    switch (abs_pdg) {
    case 11:  // electron/positron
    case 22:  // photon
    case 111: // pi0 (decays to 2 gamma)
      return true;
    default:
      return false;
    }
  }

  /// Returns charge sign from PDG code (-1, 0, +1)
  inline short int charge_from_pdg(int pdg) {
    switch (pdg) {
    // Positive particles
    case -11:   // positron
    case -13:   // anti-muon
    case 211:   // pi+
    case 321:   // K+
    case 2212:  // proton
      return +1;
    // Negative particles
    case 11:    // electron
    case 13:    // muon
    case -211:  // pi-
    case -321:  // K-
    case -2212: // anti-proton
      return -1;
    // Neutral particles
    default:
      return 0;
    }
  }

  /// Create SRTrack from SRTrueParticle (fake reconstruction)
  inline ::caf::SRTrack SRTrack_from_true_particle(const ::caf::SRTrueParticle& true_particle,
                                                   const ::caf::TrueParticleID& particle_id) {
    ::caf::SRTrack track{};

    track.start = true_particle.start_pos;
    track.end   = true_particle.end_pos;

    // Direction from momentum
    track.dir    = normalize_to_direction(true_particle.p.px, true_particle.p.py, true_particle.p.pz);
    track.enddir = track.dir; // Same direction for fake reco

    track.time = true_particle.time;

    // Energy in MeV (SRTrack expects MeV, SRTrueParticle.p is in GeV)
    track.E    = true_particle.p.E * 1000.0f;
    track.Evis = track.E;

    // Track length
    const float dx = track.end.x - track.start.x;
    const float dy = track.end.y - track.start.y;
    const float dz = track.end.z - track.start.z;
    track.len_cm = std::sqrt(dx * dx + dy * dy + dz * dz);

    track.charge = charge_from_pdg(true_particle.pdg);
    track.qual   = 1.0f; // Perfect quality for fake reco

    // Truth association (100% overlap)
    track.truth.push_back(particle_id);
    track.truthOverlap.push_back(1.0f);

    return track;
  }

  /// Create SRShower from SRTrueParticle (fake reconstruction)
  inline ::caf::SRShower SRShower_from_true_particle(const ::caf::SRTrueParticle& true_particle,
                                                     const ::caf::TrueParticleID& particle_id) {
    ::caf::SRShower shower{};

    shower.start = true_particle.start_pos;

    // Direction from momentum
    shower.direction = normalize_to_direction(true_particle.p.px, true_particle.p.py, true_particle.p.pz);

    // Energy in MeV (SRShower.Evis is in MeV based on default value pattern)
    shower.Evis = true_particle.p.E * 1000.0f;

    // Truth association (100% overlap)
    shower.truth.push_back(particle_id);
    shower.truthOverlap.push_back(1.0f);

    return shower;
  }

  /// Create SRRecoParticle from SRTrueParticle (fake reconstruction)
  inline ::caf::SRRecoParticle SRRecoParticle_from_true_particle(const ::caf::SRTrueParticle& true_particle,
                                                                  const ::caf::TrueParticleID& particle_id) {
    ::caf::SRRecoParticle reco_particle{};

    reco_particle.primary = true;
    reco_particle.pdg     = true_particle.pdg;
    reco_particle.score   = 1.0f; // Perfect PID for fake reco

    // Energy in GeV (SRLorentzVector has public member E)
    reco_particle.E        = true_particle.p.E;
    reco_particle.E_method = ::caf::PartEMethod::kCalorimetry;

    // Momentum (SRLorentzVector has public members px, py, pz)
    reco_particle.p = ::caf::SRVector3D{true_particle.p.px, true_particle.p.py, true_particle.p.pz};

    reco_particle.start = true_particle.start_pos;
    reco_particle.end   = true_particle.end_pos;

    // Determine if track or shower
    if (is_track_like(true_particle.pdg)) {
      reco_particle.origRecoObjType = ::caf::RecoObjType::kTrack;
    } else if (is_shower_like(true_particle.pdg)) {
      reco_particle.origRecoObjType = ::caf::RecoObjType::kShower;
    }

    // Truth association (100% overlap)
    reco_particle.truth.push_back(particle_id);
    reco_particle.truthOverlap.push_back(1.0f);

    return reco_particle;
  }

} // namespace sand::fake_reco

#endif // SANDRECO_FAKE_RECO_TRACKS_SHOWERS_HPP
