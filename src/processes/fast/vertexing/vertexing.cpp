#include "vertexing.hpp"

#include <caf/caf_wrapper.hpp>

#include <ufw/factory.hpp>

#include <cstddef>
#include <vector>

namespace sand::fast {

  vertexing::vertexing()
    : process{
          {{"in_truth", "sand::caf::truth_branch_wrapper"},
           {"in_assns", "sand::caf::sand_assn_wrapper"},
           {"in_grain", "sand::caf::grain_reco_wrapper"},
           {"in_tracker", "sand::caf::tracker_reco_wrapper"},
           {"in_ecal", "sand::caf::ecal_reco_wrapper"}},
          {{"out_common", "sand::caf::common_reco_branch_wrapper"}, {"out_nd", "sand::caf::nd_reco_branch_wrapper"}}} {}

  void vertexing::run() {
    auto const& truth   = get<sand::caf::truth_branch_wrapper>("in_truth");
    auto const& assns   = get<sand::caf::sand_assn_wrapper>("in_assns");
    auto const& grain   = get<sand::caf::grain_reco_wrapper>("in_grain");
    auto const& tracker = get<sand::caf::tracker_reco_wrapper>("in_tracker");
    auto const& ecal    = get<sand::caf::ecal_reco_wrapper>("in_ecal");
    auto& common        = set<sand::caf::common_reco_branch_wrapper>("out_common");
    auto& sand          = set<sand::caf::nd_reco_branch_wrapper>("out_nd").sand;

    // 1. The subdetector objects: the constituents of the assns point into these collections
    sand.grain   = grain;
    sand.tracker = tracker;
    sand.ecal    = ecal;

    // 2. The interactions come from the assns: one per true interaction with at least one assn, at the true vertex.
    //    A neutrino that left nothing visible gives no interaction. common.ixn.sandreco[i] and sand.ixn[i] are created
    //    together, so SRRecoBaseID{i, kSANDAssn, k} resolves to sand.ixn[i].trkmatch[k].
    std::vector<int> reco_of_true(truth.nu.size(), -1); // true interaction -> reco interaction, -1 if none yet
    for (auto const& assn : assns) {
      auto const true_ixn = static_cast<std::size_t>(assn.trk.truth.at(0).ixn);
      if (reco_of_true.at(true_ixn) < 0) {
        reco_of_true[true_ixn] = static_cast<int>(common.ixn.sandreco.size());

        auto& ixn        = common.ixn.sandreco.emplace_back();
        ixn.id           = truth.nu[true_ixn].id;
        ixn.vtx          = truth.nu[true_ixn].vtx;
        ixn.vtx_time     = truth.nu[true_ixn].time; // ns
        ixn.truth        = {true_ixn};
        ixn.truthOverlap = {1.f};
        sand.ixn.emplace_back();
      }

      // 3. Every assn goes to the interaction of its particle, keeping the order of the matching
      auto& sand_ixn = sand.ixn[static_cast<std::size_t>(reco_of_true[true_ixn])];
      sand_ixn.trkmatch.push_back(assn);
      sand_ixn.ntrkmatch = sand_ixn.trkmatch.size();
    }
    common.ixn.nsandreco = common.ixn.sandreco.size();
    sand.nixn            = sand.ixn.size();

    UFW_DEBUG("fast::vertexing: {} interactions out of {} neutrinos, {} assns", sand.nixn, truth.nu.size(),
              assns.size());
  }

} // namespace sand::fast

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::fast::vertexing);
