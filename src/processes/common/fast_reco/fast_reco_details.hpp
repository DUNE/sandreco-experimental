#ifndef SAND_COMMON_FAST_RECO_DETAILS_HPP
#define SAND_COMMON_FAST_RECO_DETAILS_HPP

#include <duneanaobj/StandardRecord/SREnums.h>

#include <vector>

namespace caf {
  class SRVector3D;
  class SRNeutrinoHypothesisBranch;
  class SRTrueInteraction;
  class SRDirectionBranch;
  class SRNeutrinoEnergyBranch;
  class SRRecoParticlesBranch;
  class SRRecoParticle;
  class SRTrueParticle;
  class SRShower;
  class SRTrack;
  class SRTracker;
} // namespace caf

namespace sand::common::reco_details {

  /// Where one true particle (prim or sec) lands in the output: its position in
  /// SRRecoParticlesBranch::sandreco, and, if track-/shower-like, in SRTracker::tracks/showers.
  struct ParticleSlot {
    ::caf::TrueParticleID id;
    int part_idx;
    int track_idx  = -1; ///< valid only if track-like
    int shower_idx = -1; ///< valid only if shower-like
  };

  using ParticleSlots = std::vector<ParticleSlot>;

  /// Perfect classifier neutrino hypothesis
  [[nodiscard]] ::caf::SRNeutrinoHypothesisBranch
  neutrino_hypothesis_from_true(::caf::SRTrueInteraction const& true_ixn);

  /// Perfect direction hypothesis: every estimator collapses to the true neutrino direction,
  [[nodiscard]] ::caf::SRDirectionBranch direction_from_true(::caf::SRTrueInteraction const& true_ixn);

  /// Perfect neutrino energy hypothesis: every estimator collapses to the true energy
  [[nodiscard]] ::caf::SRNeutrinoEnergyBranch energy_from_true(::caf::SRTrueInteraction const& true_ixn);

  /// Perfect SRRecoParticle: every field copied/derived 1:1 from true_part. Does not set
  /// recoobj/parent/daughters -- those are cross-particle links, resolved in reco_particles_from_true().
  [[nodiscard]] ::caf::SRRecoParticle reco_particle_from_true(::caf::SRTrueParticle const& true_part,
                                                              ::caf::TrueParticleID const& id);

  /// Perfect SRTrack, for track-like particles.
  [[nodiscard]] ::caf::SRTrack track_from_true(::caf::SRTrueParticle const& true_part, ::caf::TrueParticleID const& id);

  /// Perfect SRShower, for shower-like particles.
  [[nodiscard]] ::caf::SRShower shower_from_true(::caf::SRTrueParticle const& true_part,
                                                 ::caf::TrueParticleID const& id);

  /// Indexes every particle of true_ixn.prim then true_ixn.sec, in order: the position each one will
  /// get in SRRecoParticlesBranch::sandreco / SRTracker::tracks / SRTracker::showers. Computed once and
  /// shared by reco_particles_from_true()/sand_tracker_from_true() so their cross-links
  /// (SRRecoParticle::recoobj <-> SRTrack/SRShower::part) can never fall out of sync.
  /// @param ixn_idx  index of this interaction in the SRTruthBranch (-> TrueParticleID::ixn).
  [[nodiscard]] ParticleSlots particle_slots_from_true(::caf::SRTrueInteraction const& true_ixn, int ixn_idx);

  /// Perfect SRRecoParticlesBranch: one SRRecoParticle per true_ixn.prim/sec, with recoobj (link to the
  /// matching SRTrack/SRShower from sand_tracker_from_true()) and parent/daughters (from
  /// true_part.parentID) resolved.
  [[nodiscard]] ::caf::SRRecoParticlesBranch reco_particles_from_true(::caf::SRTrueInteraction const& true_ixn,
                                                                      int ixn_idx);

  /// Perfect SRTracker: one SRTrack/SRShower per track-/shower-like true_ixn.prim/sec, with part set to
  /// link back to the matching SRRecoParticle from reco_particles_from_true().
  [[nodiscard]] ::caf::SRTracker sand_tracker_from_true(::caf::SRTrueInteraction const& true_ixn, int ixn_idx);

} // namespace sand::common::reco_details

#endif
