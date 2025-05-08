#pragma once

#include <geomanager/tracker_manager.hpp>

namespace sand {

  class geomanager::drift_manager : public tracker_manager {

  public:
    drift_manager(const geomanager&);

  };

}
