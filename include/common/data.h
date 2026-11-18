#pragma once

#include <common/version.h>

#include <cstdint>
#include <ufw/data.hpp>
#include <ufw/store.hpp>

#include <set>
#include <vector>


namespace sand {

  using managed_data_base = ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag>;

  template <typename T>
  struct spill_data_collection
    : public managed_data_base
    , public std::vector<T> {
    using type = T;
  };

  // Shared-object collections are ufw::data::managed_collection (see
  // SAND_SHARED_COLLECTION below); no sandreco-side counterpart is needed.

} // namespace sand

#include <ufw/index.hpp>

class EDEPHit;

namespace sand {

  struct truth_adapter {
    using value_type = const EDEPHit;
    using index_type = std::size_t;
    static value_type& at(const index_type&);
    static bool valid(const index_type&);
  };

  using truth_index = ufw::data::index<truth_adapter, std::size_t>;

} // namespace sand

namespace sand {

  /**
   * Base class for a collection of objects that are built from MC truth.
   */
  template <typename T = sand::truth_index>
  class truth {
    static_assert(std::is_base_of_v<sand::truth_index, T>, "T must be or derive from sand::truth_index");

   public:
    using true_hit_type = T;

   public:
    truth() = default;
    truth(true_hit_type onehit) : m_hits{onehit} {}
    const std::set<true_hit_type>& true_hits() const { return m_hits; }
    inline void emplace(true_hit_type&& i) { m_hits.emplace(std::move(i)); }
    inline void insert(true_hit_type i) { m_hits.emplace(i); }
    inline void insert(const std::set<true_hit_type>& set) { m_hits.insert(set.begin(), set.end()); }
    inline std::size_t size() const { return m_hits.size(); }
    inline bool empty() const { return m_hits.empty(); }

   private:
    std::set<true_hit_type> m_hits;
  };

} // namespace sand

UFW_DECLARE_UNMANAGED_DATA(ufw::context_id)
UFW_DECLARE_UNMANAGED_DATA(ufw::data::data_base)
UFW_DECLARE_UNMANAGED_DATA(ufw::data::managed_tag)
UFW_DECLARE_UNMANAGED_DATA(ufw::data::instanced_tag)
UFW_DECLARE_UNMANAGED_DATA(ufw::data::context_tag)

UFW_DECLARE_UNMANAGED_DATA(sand::managed_data_base)
UFW_DECLARE_UNMANAGED_DATA(sand::truth_index)

#define SAND_DATA_COLLECTION(NS, T, NAME)                                                                              \
  UFW_DECLARE_UNMANAGED_DATA(NS::T)                                                                                    \
  namespace NS {                                                                                                       \
    using NAME = sand::spill_data_collection<NS::T>;                                                                   \
  }                                                                                                                    \
  UFW_DECLARE_MANAGED_DATA(NS::NAME)

/**
 * Declare a per-context store of shared objects addressed through typed keys, the
 * shared-store counterpart of SAND_DATA_COLLECTION: NS::NAME becomes the collection type
 * and the registered (streamed) name of the managed variable. UFW_DECLARE_STORE adds the
 * NS::NAME_store() accessor and the NS::NAME_index key type, and registers the key as
 * unmanaged data. NAME also becomes the public id of the store variable (the name used
 * in a process's reqs/prods and as a branch name); the public id space is flat —
 * namespaces do not qualify it — so NAME must be unique across all stores.
 */
#define SAND_SHARED_COLLECTION(NS, T, NAME)                                                                            \
  UFW_DECLARE_UNMANAGED_DATA(NS::T)                                                                                    \
  namespace NS {                                                                                                       \
    using NAME = ufw::data::managed_collection<NS::T>;                                                                 \
  }                                                                                                                    \
  UFW_DECLARE_STORE(NS, NS::NAME, NAME)
