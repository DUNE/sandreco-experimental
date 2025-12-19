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

  class truth {
    
  public:
    truth() = default;
    truth(truth_index onehit) : m_hits{onehit} {}
    const std::set<truth_index> true_hits() const { return m_hits; }
    inline void insert(truth_index i) { m_hits.emplace(i); }
    inline void insert(const std::set<truth_index>& set) { m_hits.insert(set.begin(), set.end()); }

  private:
    std::set<truth_index> m_hits;

  };

} // namespace sand
