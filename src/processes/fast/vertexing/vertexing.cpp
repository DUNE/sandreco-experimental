#include "vertexing.hpp"

#include <caf/caf_wrapper.hpp>

#include <ufw/factory.hpp>

#include <cstddef>

namespace sand::fast {

  vertexing::vertexing()
    : process{
          {{"in_truth", "sand::caf::truth_branch_wrapper"},
           {"in_assns", "sand::caf::sand_assn_wrapper"},
           {"in_tracker", "sand::caf::tracker_reco_wrapper"}},
          {{"out_common", "sand::caf::common_reco_branch_wrapper"}, {"out_nd", "sand::caf::nd_reco_branch_wrapper"}}} {}

  void vertexing::run() {
    auto const& truth   = get<sand::caf::truth_branch_wrapper>("in_truth");
    auto const& assns   = get<sand::caf::sand_assn_wrapper>("in_assns");
    auto const& tracker = get<sand::caf::tracker_reco_wrapper>("in_tracker");
    auto& common        = set<sand::caf::common_reco_branch_wrapper>("out_common");
    auto& sand          = set<sand::caf::nd_reco_branch_wrapper>("out_nd").sand;

    // 1. The subdetector objects: the constituents of the assns point into these collections
    sand.tracker = tracker;

    // 2. One reco interaction per true interaction, at the true vertex
    for (std::size_t i{}; i != truth.nu.size(); ++i) {
      auto& ixn        = common.ixn.sandreco.emplace_back();
      ixn.id           = truth.nu[i].id;
      ixn.vtx          = truth.nu[i].vtx;
      ixn.truth        = {i};
      ixn.truthOverlap = {1.f};
    }
    common.ixn.nsandreco = common.ixn.sandreco.size();

    // Index-aligned with common.ixn.sandreco: SRRecoBaseID{i, kSANDAssn, k} resolves to sand.ixn[i].trkmatch[k]
    sand.ixn.resize(truth.nu.size());
    sand.nixn = sand.ixn.size();

    // 3. Every assn goes to the interaction of its particle, keeping the order of the matching
    for (auto const& assn : assns) {
      auto& sand_ixn = sand.ixn.at(static_cast<std::size_t>(assn.trk.truth.at(0).ixn));
      sand_ixn.trkmatch.push_back(assn);
      sand_ixn.ntrkmatch = sand_ixn.trkmatch.size();
    }

    UFW_DEBUG("fast::vertexing: {} interactions, {} assns", sand.nixn, assns.size());
  }

} // namespace sand::fast

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::fast::vertexing);
