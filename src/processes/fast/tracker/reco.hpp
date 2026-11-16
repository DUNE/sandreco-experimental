#ifndef SAND_FAST_TRACKER_RECO_HPP
#define SAND_FAST_TRACKER_RECO_HPP

#include <common/version.h>

#include <ufw/process.hpp>

namespace sand::fast::tracker {

  class reco : public ufw::process {
    double m_min_energy_deposit{};
    double m_b_field{};

   public:
    reco();
    void configure(ufw::config const& cfg) override;
    void run() override;
  };

} // namespace sand::fast::tracker

UFW_REGISTER_PROCESS(sand::fast::tracker::reco);

#endif
