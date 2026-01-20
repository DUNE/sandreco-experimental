//
// Created by root on 1/20/26.
//

#ifndef SANDRECO_READ_CAF_HPP
#define SANDRECO_READ_CAF_HPP

#include <ufw/process.hpp>

#include <caf/caf_wrapper.hpp>

namespace sand::fake_reco {

  class read_caf : public ufw::process {
    sand::caf::caf_wrapper* standard_record_{};
  public:
    read_caf();

    void configure(const ufw::config& cfg) override;
    void run() override;
  };
} // namespace sand::fake_reco

UFW_REGISTER_PROCESS(sand::fake_reco::read_caf);

#endif // SANDRECO_READ_CAF_HPP
