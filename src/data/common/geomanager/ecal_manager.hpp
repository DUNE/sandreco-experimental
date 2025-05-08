#pragma once

#include <geomanager/subdetector_manager.hpp>

namespace sand {

  class geomanager::ecal_manager : public subdetector_manager {

  public:
    ecal_manager(const geomanager&);

  };

}
