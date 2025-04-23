#pragma once

#include <set>

#ifndef __CLING__
#include <ufw/index.hpp>

class TG4HitSegment;

namespace sand {

  extern const TG4HitSegment& get_truth_at(const size_t& i);
  extern bool check_truth_at(const size_t& i);

  using truth_adapter = ufw::data::collection_adapter<const TG4HitSegment, size_t, get_truth_at, check_truth_at>;
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
