#include "shared_store_resolve.hpp"

#include <common/shared.h>

#include <ufw/factory.hpp>

#include <cstddef>

namespace sand::common {

  shared_store_resolve::shared_store_resolve()
    : process({{"record", "sand::shared_record"}, {"store", "sand::shared"}}, {}) {}

  void shared_store_resolve::run() {
    // Touch the record first, before the store is ever used: this exercises the late-attach
    // read path, since the store branch is only attached (and re-read) on first key use below.
    const auto& rec = get<shared_record>("record");
    const auto& k   = rec.key();
    const auto idx  = k.get();
    UFW_ASSERT(idx == 1, "The key in the record must point at element 1, was {}.", idx);
    UFW_ASSERT(static_cast<bool>(k), "The key in the record must be valid after read-back.");
    // The key must resolve against the read-back store: to the very element it points at.
    const auto& st = sand::shared_store();
    UFW_ASSERT(st.size() == 3, "The store must read back with the 3 written elements, was {}.", st.size());
    UFW_ASSERT(&*k == &st.at(1), "The key must resolve to element 1 of the read-back store.");
  }

} // namespace sand::common

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::common::shared_store_resolve);
