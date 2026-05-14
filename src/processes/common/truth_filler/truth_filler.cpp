#include "truth_filler.hpp"

#include <ufw/factory.hpp>

namespace sand {

  truth_filler::truth_filler()
    : process{{}, {{"output_caf", "sand::caf::caf_wrapper"}}} {}

  void truth_filler::configure(ufw::config const& cfg) { process::configure(cfg); }

  void truth_filler::run() {
    auto const& m_genie = &get<genie_reader>();
    auto const& m_edep  = &get<edep_reader>();
    auto m_caf         = &set<sand::caf::caf_wrapper>("output_caf");
  }

} // namespace sand

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::truth_filler);
