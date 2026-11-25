#ifndef SAND_FAST_NEUTRINO_HPP
#define SAND_FAST_NEUTRINO_HPP

#include <common/version.h>

#include <ufw/process.hpp>

namespace sand::fast {

  struct neutrino : public ufw::process {
    neutrino();
    void run() override;
  };

} // namespace sand::fast

UFW_REGISTER_PROCESS(sand::fast::neutrino);

#endif
