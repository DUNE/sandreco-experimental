#ifndef SAND_COMMON_FAST_VERTEXING_HPP
#define SAND_COMMON_FAST_VERTEXING_HPP

#include <duneanaobj/StandardRecord/SRInteraction.h>
#include <duneanaobj/StandardRecord/SRSAND.h>

#include <vector>

namespace caf {
  class SRTruthBranch;
} // namespace caf

namespace sand::common {

  /// The reco interactions of a spill, index-aligned: interactions[i] goes to common.ixn.sandreco[i], sand[i] to
  /// nd.sand.ixn[i]
  struct Vertices {
    std::vector<::caf::SRInteraction> interactions; ///< id, vtx, truth; particles and estimates are filled later
    std::vector<::caf::SRSANDInt> sand;             ///< the SRSANDAssns of each interaction
  };

  /// Perfect vertexing: one interaction per true interaction, at the true vertex; every SRSANDAssn goes to the
  /// interaction of its trk.truth[0], keeping the order of `assns`
  Vertices vertex_by_truth(::caf::SRTruthBranch const& truth, std::vector<::caf::SRSANDAssn> assns);

} // namespace sand::common

#endif
