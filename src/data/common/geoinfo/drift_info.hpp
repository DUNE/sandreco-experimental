#pragma once

#include <geoinfo/tracker_info.hpp>

namespace sand {

  class geoinfo::drift_info : public tracker_info {

  public:
    drift_info(const geoinfo&);

  };

}
