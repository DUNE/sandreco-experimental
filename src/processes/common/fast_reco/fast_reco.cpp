#include "fast_reco.hpp"
#include "fast_reco_details.hpp"

#include <caf/caf_wrapper.hpp>

#include <duneanaobj/StandardRecord/SRInteraction.h>

#include <ufw/factory.hpp>

namespace sand::common {

  fast_reco::fast_reco()
    : process{
          {{"in_truth", "sand::caf::truth_branch_wrapper"}},
          {{"out_common", "sand::caf::common_reco_branch_wrapper"}, {"out_nd", "sand::caf::nd_reco_branch_wrapper"}}} {}

  void fast_reco::run() {
    auto const& truth_branch = get<sand::caf::truth_branch_wrapper>("in_truth");
    auto& common_reco_branch = set<sand::caf::common_reco_branch_wrapper>("out_common");
    auto& nd_reco_branch     = set<sand::caf::nd_reco_branch_wrapper>("out_nd");

    // Reserve reco capacities to match truth
    common_reco_branch.ixn.sandreco.reserve(truth_branch.nu.size());
    nd_reco_branch.sand.ixn.reserve(truth_branch.nu.size());

    for (std::size_t ixn_idx{}, n_nu = truth_branch.nu.size(); ixn_idx != n_nu; ++ixn_idx) {
      auto& true_ixn = truth_branch.nu[ixn_idx];

      auto& common_reco_ixn = common_reco_branch.ixn.sandreco.emplace_back();
      common_reco_ixn.id    = true_ixn.id;
      common_reco_ixn.vtx   = true_ixn.vtx;
      common_reco_ixn.dir   = reco_details::direction_from_true(true_ixn);
      common_reco_ixn.nuhyp = reco_details::neutrino_hypothesis_from_true(true_ixn);
      common_reco_ixn.Enu   = reco_details::energy_from_true(true_ixn);
      // Missing reco particles ...
      common_reco_ixn.truth        = {ixn_idx};
      common_reco_ixn.truthOverlap = {1.f};
    }
  }

} // namespace sand::common

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::common::fast_reco);
