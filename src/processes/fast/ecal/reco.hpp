#ifndef SAND_FAST_ECAL_RECO_HPP
#define SAND_FAST_ECAL_RECO_HPP

#include <common/version.h>

#include <ufw/process.hpp>

namespace sand::fast::ecal {

  class reco : public ufw::process {
    double m_min_energy_deposit{};

   public:
    reco();
    void configure(ufw::config const& cfg) override;
    void run() override;
  };

} // namespace sand::fast::ecal

UFW_REGISTER_PROCESS(sand::fast::ecal::reco);

#endif
