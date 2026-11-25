#ifndef SAND_FAST_PID_HPP
#define SAND_FAST_PID_HPP

#include <common/version.h>

#include <ufw/process.hpp>

namespace sand::fast {

  struct pid : public ufw::process {
    pid();
    void run() override;
  };

} // namespace sand::fast

UFW_REGISTER_PROCESS(sand::fast::pid);

#endif
