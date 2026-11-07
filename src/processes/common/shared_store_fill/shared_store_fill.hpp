#ifndef SAND_COMMON_SHARED_STORE_FILL_HPP
#define SAND_COMMON_SHARED_STORE_FILL_HPP

#include <common/version.h>

#include <ufw/process.hpp>

namespace sand::common {

  /// Creates the per-context shared-object store, fills it and mints a key into the record it produces.
  struct shared_store_fill : public ufw::process {
    shared_store_fill();
    void run() override;
  };

} // namespace sand::common

UFW_REGISTER_PROCESS(sand::common::shared_store_fill);

#endif
