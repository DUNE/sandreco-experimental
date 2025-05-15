//
// Created by paolo on 08/05/2025.
//

#ifndef FAKE_RECO_HPP
#define FAKE_RECO_HPP

#include <ufw/process.hpp>

namespace sand::fake_reco {
  class fake_reco : public ufw::process {
  public:
    fake_reco();

    void configure (const ufw::config& cfg) override;
    void run() override;
  };
} // namespace sand::fake_reco

UFW_REGISTER_PROCESS(sand::fake_reco::fake_reco);

#endif //FAKE_RECO_HPP
