#pragma once

#include <ufw/index.hpp>

namespace sand {

  struct true_hit {
    int hit;
  };

  struct true_hits {
    std::vector<int> hits;
    inline void add(int i) { hits.push_back(i);}
  };


}
