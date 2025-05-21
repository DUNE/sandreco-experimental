#pragma once

#include <geoinfo/subdetector_info.hpp>

namespace sand {

  class geoinfo::tracker_info : public subdetector_info {

  public:
    struct wire {

      pos_3d head; ///< The readout end of the wire
      pos_3d tail; ///< The termination end of the wire
      double length; ///< norm(head-tail), cached
      double sagitta; ///< Maximum deflection downwards at the centre

    };

  public:
    tracker_info(const geoinfo&, const geo_path&);

  };

}
