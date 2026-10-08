#include "fast_reco.hpp"
#include "fast_reco_details.hpp"

#include <caf/caf_wrapper.hpp>
#include <edep_reader/edep_reader.hpp>

#include <duneanaobj/StandardRecord/SRInteraction.h>

#include <ufw/factory.hpp>

namespace sand::common {

  fast_reco::fast_reco()
    : process{
          {{"in_truth", "sand::caf::truth_branch_wrapper"}},
          {{"out_common", "sand::caf::common_reco_branch_wrapper"}, {"out_nd", "sand::caf::nd_reco_branch_wrapper"}}} {}

  void fast_reco::run() {
    auto const& edep         = instance<sand::edep_reader>();
    auto const& truth_branch = get<sand::caf::truth_branch_wrapper>("in_truth");
    auto& common_reco_branch = set<sand::caf::common_reco_branch_wrapper>("out_common");
    auto& nd_reco_branch     = set<sand::caf::nd_reco_branch_wrapper>("out_nd");

    // Reserve reco capacities to match truth
    common_reco_branch.ixn.sandreco.reserve(truth_branch.nu.size());
    nd_reco_branch.sand.ixn.reserve(truth_branch.nu.size());

    auto const tracker_ids = reco_details::tracker_g4ids_from_edep(edep);

    auto& sand_tracker = nd_reco_branch.sand.tracker;

    for (std::size_t ixn_idx{}, n_nu = truth_branch.nu.size(); ixn_idx != n_nu; ++ixn_idx) {
      auto& true_ixn = truth_branch.nu[ixn_idx];

      auto const first_track_idx  = static_cast<int>(sand_tracker.tracks.size());
      auto const first_shower_idx = static_cast<int>(sand_tracker.showers.size());

      auto& common_reco_ixn = common_reco_branch.ixn.sandreco.emplace_back();
      common_reco_ixn.id    = true_ixn.id;
      common_reco_ixn.vtx   = true_ixn.vtx;
      common_reco_ixn.dir   = reco_details::direction_from_true(true_ixn);
      common_reco_ixn.nuhyp = reco_details::neutrino_hypothesis_from_true(true_ixn);
      common_reco_ixn.Enu   = reco_details::energy_from_true(true_ixn);
      common_reco_ixn.part =
          reco_details::reco_particles_from_true(true_ixn, tracker_ids, ixn_idx, first_track_idx, first_shower_idx);
      common_reco_ixn.truth        = {ixn_idx};
      common_reco_ixn.truthOverlap = {1.f};
      ++common_reco_branch.ixn.nsandreco;

      reco_details::sand_tracker_from_true(sand_tracker, true_ixn, tracker_ids, ixn_idx, edep);

      // No cross-subdetector matching here: the SAND interaction is kept only to stay index-aligned with sandreco
      nd_reco_branch.sand.ixn.emplace_back();
      ++nd_reco_branch.sand.nixn;

      UFW_ASSERT(common_reco_branch.ixn.sandreco.size() == common_reco_branch.ixn.nsandreco,
                 "common.ixn.nsandreco ({}) doesn't match common.ixn.sandreco.size() ({})",
                 common_reco_branch.ixn.nsandreco, common_reco_branch.ixn.sandreco.size());

      UFW_ASSERT(nd_reco_branch.sand.ixn.size() == nd_reco_branch.sand.nixn,
                 "nd.sand.nixn ({}) doesn't match nd.sand.ixn.size() ({})", nd_reco_branch.sand.nixn,
                 nd_reco_branch.sand.ixn.size());
    }
  }

} // namespace sand::common

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::common::fast_reco);
