#ifndef SAND_COMMON_TRUTH_MATCHING_HPP
#define SAND_COMMON_TRUTH_MATCHING_HPP

#include <vector>

namespace caf {
  class SRSAND;
  class SRSANDAssn;
} // namespace caf

namespace sand::common {

  /// Perfect matching: one SRSANDAssn per true particle, with that particle's subdetector objects as constituents.
  /// trk.truth carries the particle, as a real matcher's truth matching would; the rest of trk is not filled yet.
  /// The assns are ordered by particle: interaction, then primaries before secondaries, then position.
  std::vector<::caf::SRSANDAssn> match_by_truth(::caf::SRSAND const& objects);

} // namespace sand::common

#endif
