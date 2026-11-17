#ifndef SAND_FAST_VERTEXING_HPP
#define SAND_FAST_VERTEXING_HPP

#include <common/version.h>

#include <ufw/process.hpp>

namespace sand::fast {

  struct vertexing : public ufw::process {
    vertexing();
    void run() override;
  };

} // namespace sand::fast

UFW_REGISTER_PROCESS(sand::fast::vertexing);

#endif
