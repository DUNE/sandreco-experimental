#pragma once

#include <set>

#ifndef __CLING__
#  include <ufw/index.hpp>

class TG4HitSegment;

namespace sand {

  struct truth_adapter {
    using value_type = const TG4HitSegment;
    using index_type = std::size_t;
    static value_type& at(const index_type&);
    static bool valid(const index_type&);
  };

  using truth_index = ufw::data::index<truth_adapter>;

} // namespace sand

#else  //__CLING__

namespace sand {

  using truth_index = std::size_t;
  
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

  private:
    std::set<true_hit_type> m_hits;

  };

} // namespace sand
