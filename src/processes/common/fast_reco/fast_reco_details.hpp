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
} // namespace caf

namespace sand::common::reco_details {

  struct ParticleSlot {
    ::caf::TrueParticleID id;
    int part_idx;
    int track_idx  = -1;
    int shower_idx = -1;
  };

  using ParticleSlots = std::vector<ParticleSlot>;

  /// Perfect classifier neutrino hypothesis
  [[nodiscard]] ::caf::SRNeutrinoHypothesisBranch
  neutrino_hypothesis_from_true(::caf::SRTrueInteraction const& true_ixn);

  /// Perfect direction hypothesis: every estimator collapses to the true neutrino direction,
  [[nodiscard]] ::caf::SRDirectionBranch direction_from_true(::caf::SRTrueInteraction const& true_ixn);

  /// Perfect neutrino energy hypothesis: every estimator collapses to the true energy
  [[nodiscard]] ::caf::SRNeutrinoEnergyBranch energy_from_true(::caf::SRTrueInteraction const& true_ixn);

  [[nodiscard]] ::caf::SRRecoParticle reco_particle_from_true(::caf::SRTrueParticle const& true_part,
                                                              ::caf::TrueParticleID const& id);

  [[nodiscard]] ::caf::SRTrack track_from_true(::caf::SRTrueParticle const& true_part, ::caf::TrueParticleID const& id);

  [[nodiscard]] ::caf::SRShower shower_from_true(::caf::SRTrueParticle const& true_part,
                                                 ::caf::TrueParticleID const& id);

  [[nodiscard]] ParticleSlots particle_slots_from_true(::caf::SRTrueInteraction const& true_ixn, int ixn_idx);

} // namespace sand::common::reco_details

#endif
