#ifndef SAND_COMMON_FAST_MATCHING_DETAILS_HPP
#define SAND_COMMON_FAST_MATCHING_DETAILS_HPP

#include <duneanaobj/StandardRecord/SREnums.h>

#include <vector>

namespace caf {
  class SRNeutrinoHypothesisBranch;
  class SRTrueInteraction;
  class SRDirectionBranch;
  class SRNeutrinoEnergyBranch;
  class SRRecoParticlesBranch;
  class SRRecoParticle;
  class SRSAND;
  class SRTrueParticle;
} // namespace caf

namespace sand::common::reco_details {

  /// Perfect classifier: one-hot CVN scores on the true interaction class.
  ::caf::SRNeutrinoHypothesisBranch neutrino_hypothesis_from_true(::caf::SRTrueInteraction const& true_ixn);

  /// Perfect direction hypothesis: every estimator collapses to the true neutrino direction.
  ::caf::SRDirectionBranch direction_from_true(::caf::SRTrueInteraction const& true_ixn);

  /// Perfect neutrino energy hypothesis: every estimator collapses to the true energy.
  ::caf::SRNeutrinoEnergyBranch energy_from_true(::caf::SRTrueInteraction const& true_ixn);

  /// SAND subdetector objects that come from the same true particle.
  struct ParticleMatch {
    ::caf::TrueParticleID particle;               ///< the truth[0] shared by every constituent
    std::vector<::caf::SRSANDObjID> constituents; ///< subdetector objects, beginning to end
  };

  /// Perfect matching: groups the SAND subdetector objects by their truth[0].
  std::vector<ParticleMatch> match_by_truth(::caf::SRSAND const& sand);

  /// Perfect reco particle: every field copied or derived 1:1 from the true particle.
  ::caf::SRRecoParticle reco_particle_from_true(::caf::SRTrueParticle const& true_part,
                                                ::caf::TrueParticleID const& id);

  /// Perfect reco particles: one caf::SRRecoParticle per true `prim`/`sec`, cross-links resolved.
  ::caf::SRRecoParticlesBranch reco_particles_from_true(::caf::SRTrueInteraction const& true_ixn, int ixn_idx);
} // namespace sand::common::reco_details

#endif
