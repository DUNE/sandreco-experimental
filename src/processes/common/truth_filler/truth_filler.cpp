#include "truth_filler.hpp"
#include "truth_filler_details.hpp"

#include <caf/caf_wrapper.hpp>
#include <edep_reader/edep_reader.hpp>
#include <genie_reader/genie_reader.hpp>

#include <ufw/factory.hpp>
#include <ufw/utils.hpp>

namespace sand::common {

  truth_filler::truth_filler() : process{{}, {{"out_truth_branch", "sand::caf::truth_branch_wrapper"}}} {}

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

      // Add primaries from edep-sim
      auto [particles, ancestor_ids, nproton, nneutron, npip, npim, npi0] =
          filler_details::make_primaries(primaries, first_prim_idx, prim_count, true_ixn.id);
      true_ixn.prim     = std::move(particles);
      true_ixn.nprim    = static_cast<int>(true_ixn.prim.size());
      true_ixn.nproton  = nproton;
      true_ixn.nneutron = nneutron;
      true_ixn.npip     = npip;
      true_ixn.npim     = npim;
      true_ixn.npi0     = npi0;

      // Add secondaries from edep-sim
      true_ixn.sec =
          filler_details::make_secondaries(edep, primaries, first_prim_idx, prim_count, ancestor_ids, true_ixn.id);
      true_ixn.nsec = static_cast<int>(true_ixn.sec.size());
    }
  }

} // namespace sand::common

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::common::truth_filler);
