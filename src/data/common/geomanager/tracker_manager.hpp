#pragma once

#include <geomanager/subdetector_manager.hpp>

namespace sand {

  class geomanager::tracker_manager : public subdetector_manager {

  public:
    struct wire {

      pos_3d head; ///< The readout end of the wire
      pos_3d tail; ///< The termination end of the wire
      double length; ///< norm(head-tail), cached
      double sagitta; ///< Maximum deflection downwards at the centre

    };

  public:
    tracker_manager(const geomanager&);

  };

}
