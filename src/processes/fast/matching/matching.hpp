#ifndef SAND_FAST_MATCHING_HPP
#define SAND_FAST_MATCHING_HPP

#include <common/version.h>

#include <ufw/process.hpp>

namespace sand::fast {

  struct matching : public ufw::process {
    matching();
    void run() override;
  };

} // namespace sand::fast

UFW_REGISTER_PROCESS(sand::fast::matching)

#endif
