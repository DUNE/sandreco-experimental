#include "truth_filler.hpp"
#include "truth_filler_details.hpp"

#include <caf/caf_wrapper.hpp>

#include <ufw/factory.hpp>
#include <ufw/utils.hpp>

namespace sand::common {

  truth_filler::truth_filler() : process{{}, {{"output_caf", "sand::caf::truth_branch_wrapper"}}} {}

  void truth_filler::configure(ufw::config const& cfg) { process::configure(cfg); }

  void truth_filler::run() {
    auto const* genie  = &get<genie_reader>();
    auto const* edep   = &get<edep_reader>();
    auto* truth_branch = &set<sand::caf::truth_branch_wrapper>("output_caf");

    auto const& primaries   = edep->GetChildrenTrajectories();
    auto interaction_ranges = filler_details::make_interaction_ranges(primaries);

    auto const n_ixn = genie->events_.size();

    UFW_ASSERT(interaction_ranges.size() == n_ixn, "Mismatch between edep-sim interactions ({}) and GENIE events ({})",
               interaction_ranges.size(), n_ixn);

    truth_branch->nnu = static_cast<int>(n_ixn);
    truth_branch->nu.reserve(n_ixn);

    for (std::size_t ixn_idx{}; ixn_idx != interaction_ranges.size(); ++ixn_idx) {
      auto [first_prim_idx, prim_count] = interaction_ranges[ixn_idx];
      const auto& event                 = genie->events_[ixn_idx];
      const auto& stdhep                = genie->stdHeps_[ixn_idx];

      // Create and fill SRTrueInteraction from GENIE
      auto& true_ixn = truth_branch->nu.emplace_back(filler_details::true_interaction_from_genie(event, stdhep));

      // Add pre-FSI hadrons from GENIE StdHep
      auto prefsi      = filler_details::make_prefsi_particles(stdhep, true_ixn.id);
      true_ixn.nprefsi = static_cast<int>(prefsi.size());
      true_ixn.prefsi  = std::move(prefsi);
    }
  }

} // namespace sand::common

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::common::truth_filler);
