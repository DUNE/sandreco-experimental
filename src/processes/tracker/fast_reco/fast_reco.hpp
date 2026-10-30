#ifndef SAND_TRACKER_FAST_RECO
#define SAND_TRACKER_FAST_RECO

#include <common/version.h>

#include <ufw/process.hpp>

namespace sand::tracker {

  class fast_reco : public ufw::process {
    double m_min_energy_deposit{};

   public:
    fast_reco();
    void configure(ufw::config const& cfg) override;
    void run() override;
  };

} // namespace sand::tracker

UFW_REGISTER_PROCESS(sand::tracker::fast_reco);

#endif
