#include "fast_vertexing.hpp"

#include <duneanaobj/StandardRecord/SRTruthBranch.h>

#include <cstddef>
#include <utility>
#include <vector>

namespace sand::common {

  Vertices vertex_by_truth(::caf::SRTruthBranch const& truth, std::vector<::caf::SRSANDAssn> assns) {
    Vertices vertices;
    vertices.interactions.reserve(truth.nu.size());
    vertices.sand.resize(truth.nu.size());

    // One reco interaction per true interaction, at the true vertex
    for (std::size_t i{}; i != truth.nu.size(); ++i) {
      auto& ixn        = vertices.interactions.emplace_back();
      ixn.id           = truth.nu[i].id;
      ixn.vtx          = truth.nu[i].vtx;
      ixn.truth        = {i};
      ixn.truthOverlap = {1.f};
    }

    // Every assn goes to the interaction of its particle; the order within an interaction is the order of `assns`
    for (auto& assn : assns) {
      auto& sand_ixn = vertices.sand.at(static_cast<std::size_t>(assn.trk.truth.at(0).ixn));
      sand_ixn.trkmatch.push_back(std::move(assn));
      sand_ixn.ntrkmatch = sand_ixn.trkmatch.size();
    }

    return vertices;
  }

} // namespace sand::common
