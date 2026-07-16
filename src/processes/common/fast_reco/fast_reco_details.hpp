#ifndef SAND_COMMON_FAST_RECO_DETAILS_HPP
#define SAND_COMMON_FAST_RECO_DETAILS_HPP

#include <array>

namespace caf {
  class SRVector3D;
  class SRNeutrinoHypothesisBranch;
  class SRTrueInteraction;
  class SRDirectionBranch;
  class SRNeutrinoEnergyBranch;
} // namespace caf

namespace sand::common::reco_details {

  [[nodiscard]] ::caf::SRVector3D normalize_to_direction(float px, float py, float pz);

  /// One-hot encoding over the {0, 1, 2, N>=3} multiplicity buckets used by SRCVNScoreBranch
  [[nodiscard]] std::array<float, 4> count_bucket_one_hot(int count);

  /// Perfect classifier neutrino hypothesis
  [[nodiscard]] ::caf::SRNeutrinoHypothesisBranch
  neutrino_hypothesis_from_true(::caf::SRTrueInteraction const& true_ixn);

  /// Perfect direction hypothesis: every estimator collapses to the true neutrino direction,
  [[nodiscard]] ::caf::SRDirectionBranch direction_from_true(::caf::SRTrueInteraction const& true_ixn);

  /// Perfect neutrino energy hypothesis: every estimator collapses to the true energy
  [[nodiscard]] ::caf::SRNeutrinoEnergyBranch energy_from_true(::caf::SRTrueInteraction const& true_ixn);

} // namespace sand::common::reco_details

#endif
