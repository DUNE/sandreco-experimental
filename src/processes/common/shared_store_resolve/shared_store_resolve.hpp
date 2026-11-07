#ifndef SAND_COMMON_SHARED_STORE_RESOLVE_HPP
#define SAND_COMMON_SHARED_STORE_RESOLVE_HPP

#include <common/version.h>

#include <ufw/process.hpp>

namespace sand::common {

  /// Reads a record holding a key into the shared-object store and resolves the key.
  /// The store is never touched directly: it is attached lazily on first key use.
  struct shared_store_resolve : public ufw::process {
    shared_store_resolve();
    void run() override;
  };

} // namespace sand::common

UFW_REGISTER_PROCESS(sand::common::shared_store_resolve);

#endif
