#ifndef SAND_COMMON_FAST_RECO
#define SAND_COMMON_FAST_RECO

#include <common/version.h>

#include <ufw/process.hpp>

namespace sand::common {

  struct fast_reco : public ufw::process {
    fast_reco();
    void run() override;
  };

} // namespace sand::common

UFW_REGISTER_PROCESS(sand::common::fast_reco);

#endif
