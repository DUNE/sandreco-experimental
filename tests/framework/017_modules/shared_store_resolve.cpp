#include <common/shared.h>

#include <ufw/factory.hpp>
#include <ufw/process.hpp>

namespace sand::test {

  // Reads back the shared-object store and its record written by 016_store_write.json and
  // verifies that the record's key resolves to the right element of the read-back store.
  struct shared_store_resolve : public ufw::process {
    shared_store_resolve();
    void run() override;
  };

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

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::shared_store_resolve)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::shared_store_resolve)
