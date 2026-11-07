#include "shared_store_fill.hpp"

#include <common/shared.h>

#include <ufw/factory.hpp>

#include <cstddef>

namespace sand::common {

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

} // namespace sand::common

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::common::shared_store_fill);
