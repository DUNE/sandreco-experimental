#include "truth_filler.hpp"
#include "truth_filler_details.hpp"

#include <caf/caf_wrapper.hpp>
#include <edep_reader/edep_reader.hpp>
#include <genie_reader/genie_reader.hpp>

#include <ufw/factory.hpp>
#include <ufw/utils.hpp>

namespace sand::common {

  truth_filler::truth_filler() : process{{}, {{"out_truth_branch", "sand::caf::truth_branch_wrapper"}}} {}

  /// One SRTrueInteraction per GENIE/edep-sim interaction: vertex truth from GENIE,
  /// pre-FSI hadrons from StdHep, primaries + secondaries from the edep-sim particle tree.
  void truth_filler::run() {
    auto const& genie  = instance<genie_reader>();
    auto const& edep   = instance<edep_reader>();
    auto& truth_branch = set<sand::caf::truth_branch_wrapper>("out_truth_branch");

    auto const& primaries   = edep.GetChildrenTrajectories();
    auto interaction_ranges = filler_details::make_interaction_ranges(primaries);

    auto const n_ixn = genie.events_.size();

    UFW_ASSERT(interaction_ranges.size() == n_ixn, "Mismatch between edep-sim interactions ({}) and GENIE events ({})",
               interaction_ranges.size(), n_ixn);

    truth_branch.nnu = static_cast<int>(n_ixn);
    truth_branch.nu.reserve(n_ixn);

    for (std::size_t ixn_idx{}; ixn_idx != interaction_ranges.size(); ++ixn_idx) {
      auto [first_prim_idx, prim_count] = interaction_ranges[ixn_idx];
      auto const& event                 = genie.events_[ixn_idx];
      auto const& stdhep                = genie.stdHeps_[ixn_idx];

      // Create and fill SRTrueInteraction from GENIE
      auto& true_ixn = truth_branch.nu.emplace_back(filler_details::true_interaction_from_genie(event, stdhep));

      // Add pre-FSI hadrons from GENIE StdHep
      auto prefsi      = filler_details::make_prefsi_particles(stdhep, true_ixn.id);
      true_ixn.nprefsi = static_cast<int>(prefsi.size());
      true_ixn.prefsi  = std::move(prefsi);

      // Create primaries and secondaries tree
      auto tree = filler_details::build_true_particle_tree(primaries, first_prim_idx, prim_count,
                                                           static_cast<int>(ixn_idx), true_ixn.id);

      // Fill CAF fields
      true_ixn.prim     = std::move(tree.prim);
      true_ixn.nprim    = static_cast<int>(true_ixn.prim.size());
      true_ixn.sec      = std::move(tree.sec);
      true_ixn.nsec     = static_cast<int>(true_ixn.sec.size());
      true_ixn.nproton  = tree.nproton;
      true_ixn.nneutron = tree.nneutron;
      true_ixn.npip     = tree.npip;
      true_ixn.npim     = tree.npim;
      true_ixn.npi0     = tree.npi0;
    }
  }

} // namespace sand::common

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::common::truth_filler);
