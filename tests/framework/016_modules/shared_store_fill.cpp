#include <common/shared.h>

#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <cstddef>

namespace sand::test {

  // Creates the per-context shared-object store, fills it and mints a key into the record it
  // produces (see 016_store_write.json), so 017_store_read.json can verify the round trip.
  struct shared_store_fill : public ufw::process {
    shared_store_fill();
    void run() override;
  };

  shared_store_fill::shared_store_fill()
    : process({}, {{"record", "sand::shared_record"}, {"store", "sand::shared"}}) {}

  void shared_store_fill::run() {
    auto& st = sand::shared_store();
    UFW_ASSERT(st.empty(), "The store must be reset between contexts.");
    // Fill the store with a few placeholder elements; shared_object carries no data of its own,
    // so their only role is to give the key something to point at.
    for (std::size_t i = 0; i < 3; ++i) {
      st.push_back(shared_object{});
    }
    // Mint a key pointing at the middle element and store it in the record.
    set<shared_record>("record").set_key(shared_index{1});
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::shared_store_fill)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::shared_store_fill)
