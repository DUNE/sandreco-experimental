#include "truth_filler.hpp"

#include <ufw/factory.hpp>
#include <ufw/utils.hpp>
#include <vector>

namespace sand {

  std::vector<InteractionRange> make_interaction_ranges(Primaries const& primaries) {
    std::vector<InteractionRange> output;

    for (auto it = primaries.begin(); it != primaries.end();) {
      auto group_end = std::find_if_not(it, primaries.end(), [ixn = it->GetInteractionNumber()](auto const& p) {
        return p.GetInteractionNumber() == ixn;
      });
      output.push_back({static_cast<std::size_t>(it - primaries.begin()), static_cast<std::size_t>(group_end - it)});
      it = group_end;
    }

    return output;
  }

  truth_filler::truth_filler() : process{{}, {{"output_caf", "sand::caf::caf_wrapper"}}} {}

  void truth_filler::configure(ufw::config const& cfg) { process::configure(cfg); }

  void truth_filler::run() {
    auto const& genie = &get<genie_reader>();
    auto const& edep  = &get<edep_reader>();
    auto caf          = &set<sand::caf::caf_wrapper>("output_caf");

    auto const& primaries = edep->GetChildrenTrajectories();

    auto interaction_ranges = make_interaction_ranges(primaries);

    UFW_ASSERT(interaction_ranges.size() == genie->events_.size(),
               "Mismatch between edep-sim interactions ({}) and GENIE events ({})", interaction_ranges.size(),
               genie->events_.size());

    for (std::size_t ixn_idx{}; ixn_idx != interaction_ranges.size(); ++ixn_idx) {
      auto const& [first_prim_idx, prim_count] = interaction_ranges[ixn_idx];
      auto const& event                        = genie->events_[ixn_idx];
      auto const& stdhep                       = genie->stdHeps_[ixn_idx];
    }
  }

} // namespace sand

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::truth_filler);
