#ifndef SAND_COMMON_FAST_KINEMATICS_HPP
#define SAND_COMMON_FAST_KINEMATICS_HPP

namespace caf {
  class SRDirectionBranch;
  class SRNeutrinoEnergyBranch;
  class SRTrueInteraction;
} // namespace caf

namespace sand::common {

  /// Perfect neutrino energy hypothesis: every estimator collapses to the true energy.
  ::caf::SRNeutrinoEnergyBranch energy_from_true(::caf::SRTrueInteraction const& true_ixn);

  /// Perfect direction hypothesis: every estimator collapses to the true neutrino direction.
  ::caf::SRDirectionBranch direction_from_true(::caf::SRTrueInteraction const& true_ixn);

} // namespace sand::common

#endif
