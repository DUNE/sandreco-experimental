//
// Created by root on 1/20/26.
//
#include "read_caf.hpp"

#include <caf/caf_wrapper.hpp>
#include <ufw/factory.hpp>

namespace sand::fake_reco {

  read_caf::read_caf() : process{{{"caf_to_read", "sand::caf::caf_wrapper"}}, {}} {}

  void read_caf::configure(const ufw::config& cfg) { process::configure(cfg); }

  void read_caf::run() {
    const auto& caf = get<caf::caf_wrapper>("caf_to_read");
  }

} // namespace sand::fake_reco

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::fake_reco::read_caf);