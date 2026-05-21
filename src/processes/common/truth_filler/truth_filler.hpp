#ifndef SAND_TRUTH_FILLER_HPP
#define SAND_TRUTH_FILLER_HPP

#include <caf/caf_wrapper.hpp>
#include <edep_reader/edep_reader.hpp>
#include <genie_reader/genie_reader.hpp>

#include <ufw/process.hpp>

namespace sand {

  struct InteractionRange {
    std::size_t first_primary_index;
    std::size_t primary_count;
  };

  using Primaries = std::vector<EDEPTrajectory>;

  std::vector<InteractionRange> make_interaction_ranges(Primaries const& primaries);

  struct truth_filler : public ufw::process {
    truth_filler();
    void configure(ufw::config const& cfg) override;
    void run() override;
  };

} // namespace sand

UFW_REGISTER_PROCESS(sand::truth_filler);

#endif
