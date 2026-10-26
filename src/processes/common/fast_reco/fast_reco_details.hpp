#ifndef SAND_COMMON_FAST_RECO_DETAILS_HPP
#define SAND_COMMON_FAST_RECO_DETAILS_HPP

#include <duneanaobj/StandardRecord/SREnums.h>

namespace caf {
  class SRNeutrinoHypothesisBranch;
  class SRTrueInteraction;
  class SRDirectionBranch;
  class SRNeutrinoEnergyBranch;
  class SRRecoParticlesBranch;
  class SRRecoParticle;
  class SRTrueParticle;
} // namespace caf

namespace sand::common::reco_details {

  /// \brief Perfect classifier: one-hot CVN scores on the true interaction class.
  [[nodiscard]] ::caf::SRNeutrinoHypothesisBranch
  neutrino_hypothesis_from_true(::caf::SRTrueInteraction const& true_ixn);

  /// \brief Perfect direction hypothesis: every estimator collapses to the true neutrino direction.
  [[nodiscard]] ::caf::SRDirectionBranch direction_from_true(::caf::SRTrueInteraction const& true_ixn);

  /// \brief Perfect neutrino energy hypothesis: every estimator collapses to the true energy.
  [[nodiscard]] ::caf::SRNeutrinoEnergyBranch energy_from_true(::caf::SRTrueInteraction const& true_ixn);

  /// \brief Perfect reco particle: every field copied or derived 1:1 from the true particle.
  [[nodiscard]] ::caf::SRRecoParticle reco_particle_from_true(::caf::SRTrueParticle const& true_part,
                                                              ::caf::TrueParticleID const& id);

  /// \brief Perfect reco particles: one caf::SRRecoParticle per true `prim`/`sec`, cross-links resolved.
  [[nodiscard]] ::caf::SRRecoParticlesBranch reco_particles_from_true(::caf::SRTrueInteraction const& true_ixn,
                                                                      int ixn_idx);
} // namespace sand::common::reco_details

#endif
