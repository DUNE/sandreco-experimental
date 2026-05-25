#pragma once

#include <set>

#include <common/data.h>

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

#else  //__CLING__

namespace sand {

  struct truth_index {
    std::size_t i;
  };

  bool operator < (const truth_index& lhs, const truth_index& rhs) {
    return lhs.i < rhs.i;
  }

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
    inline std::size_t size() const { return m_hits.size();}
    inline bool empty() const { return m_hits.empty(); }

  private:
    std::set<true_hit_type> m_hits;

  };

} // namespace sand
