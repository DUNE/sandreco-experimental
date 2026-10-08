#pragma once

#include <common/version.h>

#include <ufw/data.hpp>

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

} // namespace sand

#ifndef __CLING__
#  include <ufw/index.hpp>

class EDEPHit;

namespace sand {

  struct truth_adapter {
    using value_type = const EDEPHit;
    using index_type = std::size_t;
    static value_type& at(const index_type&);
    static bool valid(const index_type&);
  };

  using truth_index = ufw::data::index<truth_adapter>;

} // namespace sand

#else //__CLING__

namespace sand {

  struct truth_index {
    std::size_t m_index;
  };

  bool operator< (const truth_index& lhs, const truth_index& rhs) { return lhs.m_index < rhs.m_index; }

} // namespace sand

#endif //__CLING__

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
