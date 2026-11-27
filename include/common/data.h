#pragma once

#include <common/version.h>

#include <cstdint>
#include <ufw/data.hpp>
#include <ufw/store.hpp>

#include <iterator>
#include <set>
#include <utility>
#include <vector>


namespace sand {

  using managed_data_base = ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag>;

  template <typename T>
  struct spill_data_collection
    : public managed_data_base
    , public std::vector<T> {
    using type = T;
  };

  /**
   * Base for a shared-object store (see SAND_SHARED_COLLECTION below).
   *
   * Named keyed_store because shared_store is taken by the accessor function of the
   * sand::shared store.
   *
   * Behaves like ufw::data::managed_collection, but the single-value insertions
   * (push_back, emplace_back, single-element insert) return the store's typed key
   * (Key) of the inserted element, so a key can only be minted by inserting into
   * the store or copied from an existing key: Key's raw-index constructor is
   * private (the store is its only friend) and its reset() is deleted.
   * The multi-element insert overloads cannot mint a single key and are deleted.
   */
  template <typename T, typename Key>
  struct keyed_store : public ufw::data::managed_collection<T> {
    using base           = ufw::data::managed_collection<T>;
    using const_iterator = typename base::const_iterator;

    Key push_back(const T& v) {
      base::push_back(v);
      return Key{this->size() - 1};
    }

    Key push_back(T&& v) {
      base::push_back(std::move(v));
      return Key{this->size() - 1};
    }

    template <class... Args>
    Key emplace_back(Args&&... args) {
      base::emplace_back(std::forward<Args>(args)...);
      return Key{this->size() - 1};
    }

    Key insert(const_iterator pos, const T& v) {
      return Key{static_cast<std::size_t>(std::distance(base::begin(), base::insert(pos, v)))};
    }

    Key insert(const_iterator pos, T&& v) {
      return Key{static_cast<std::size_t>(std::distance(base::begin(), base::insert(pos, std::move(v))))};
    }

    // Multi-element insertions cannot mint a single key and are disabled.
    void insert(const_iterator, std::size_t, const T&) = delete;
    template <class InputIt>
    void insert(const_iterator, InputIt, InputIt) = delete;
    void insert(const_iterator, std::initializer_list<T>) = delete;
  };

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
 * shared-store counterpart of SAND_DATA_COLLECTION: NS::NAME becomes the collection
 * type (a sand::keyed_store whose single-value insertions return keys) and the
 * registered (streamed) name of the managed variable.
 *
 * The macro creates, in namespace NS:
 * - NS::NAME_store(): an inline accessor returning the current context's store;
 * - NS::NAME_index: the typed key, bound to the store. It can only be obtained from
 *   a store insertion (or copied): the raw-index constructor is private (the store
 *   is the only friend) and reset() is deleted;
 * and registers the collection as managed data and the key as unmanaged data, so
 * both can be used as streamer variables.
 *
 * NAME also becomes the public id of the store variable (the name used in a
 * process's reqs/prods and as a branch name); the public id space is flat —
 * namespaces do not qualify it — so NAME must be unique across all stores.
 */
#define SAND_SHARED_COLLECTION(NS, T, NAME)                                                                              \
  UFW_DECLARE_UNMANAGED_DATA(NS::T)                                                                                      \
  namespace NS {                                                                                                         \
    struct NAME;                                                                                                         \
    inline NAME& NAME##_store();                                                                                         \
    class NAME##_index;                                                                                                  \
    struct NAME : public sand::keyed_store<T, NAME##_index> {};                                                          \
    class NAME##_index final : public ufw::data::index<ufw::data::sequential_adapter<NAME, &NAME##_store>, std::size_t> {\
      friend sand::keyed_store<T, NAME##_index>;                                                                         \
      explicit NAME##_index(std::size_t i)                                                                               \
        : ufw::data::index<ufw::data::sequential_adapter<NAME, &NAME##_store>, std::size_t>(i) {}                        \
     public:                                                                                                             \
      NAME##_index() = default;                                                                                          \
      void reset(std::size_t) = delete;                                                                                  \
    };                                                                                                                   \
    static_assert(ufw::data::is_sequential_container<NAME>,                                                             \
                  "SAND_SHARED_COLLECTION requires a collection with the sequential (at/size) interface");               \
    inline NAME& NAME##_store() { return ufw::context::current()->instance<NAME>(#NAME); }                               \
  }                                                                                                                      \
  UFW_DECLARE_MANAGED_DATA(NS::NAME)                                                                                     \
  UFW_DECLARE_UNMANAGED_DATA(NS::NAME##_index)
