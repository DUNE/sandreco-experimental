#ifndef SAND_COMMON_FAST_MATCHING
#define SAND_COMMON_FAST_MATCHING

#include <common/version.h>

#include <ufw/process.hpp>

namespace sand::common {

  struct fast_matching : public ufw::process {
    fast_matching();
    void run() override;
  };

} // namespace sand::common

UFW_REGISTER_PROCESS(sand::common::fast_matching);

#endif
