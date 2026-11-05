#ifndef SAND_COMMON_FAST_PID_HPP
#define SAND_COMMON_FAST_PID_HPP

namespace caf {
  class SRNeutrinoHypothesisBranch;
  class SRTrueInteraction;
  class SRTrueParticle;
  class SRRecoParticle;
  class SRRecoParticlesBranch;
  class TrueParticleID;
} // namespace caf

namespace sand::common {

  /// Perfect classifier: one-hot CVN scores on the true interaction class.
  ::caf::SRNeutrinoHypothesisBranch neutrino_hypothesis_from_true(::caf::SRTrueInteraction const& true_ixn);

  /// Perfect reco particle: every field copied or derived 1:1 from the true particle.
  ::caf::SRRecoParticle reco_particle_from_true(::caf::SRTrueParticle const& true_part,
                                                ::caf::TrueParticleID const& id);

  /// Perfect reco particles: one caf::SRRecoParticle per true `prim`/`sec`, cross-links resolved.
  ::caf::SRRecoParticlesBranch reco_particles_from_true(::caf::SRTrueInteraction const& true_ixn, int ixn_idx);

} // namespace sand::common

#endif
