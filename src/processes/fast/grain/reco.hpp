#ifndef SAND_FAST_GRAIN_RECO_HPP
#define SAND_FAST_GRAIN_RECO_HPP

#include <common/version.h>

#include <ufw/process.hpp>

namespace sand::fast::grain {

  class reco : public ufw::process {
    double m_min_energy_deposit{};

   public:
    reco();
    void configure(ufw::config const& cfg) override;
    void run() override;
  };

} // namespace sand::fast::grain

UFW_REGISTER_PROCESS(sand::fast::grain::reco);

#endif
