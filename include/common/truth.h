#pragma once

#include <set>

#ifndef __CLING__
#include <ufw/index.hpp>

class TG4HitSegment;

namespace sand {

  struct truth_adapter {
    using value_type = const TG4HitSegment;
    using index_type = std::size_t;
    static value_type& at(const index_type&);
    static bool valid(const index_type&);
  };

  using truth_index = ufw::data::index<truth_adapter>;

}

#endif //__CLING__

namespace sand {

  struct true_hit {
#ifdef __CLING__
    size_t hit;
#else //__CLING__
    truth_index hit;
#endif //__CLING__
  };

  struct true_hits {
#ifdef __CLING__
    std::set<size_t> hits;
    inline void add(size_t i) { hits.emplace(i); }
#else //__CLING__
    std::set<truth_index> hits;
    inline void add(truth_index i) { hits.emplace(i); }
#endif //__CLING__
  };


}
