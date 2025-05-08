#pragma once

#include <geomanager/geomanager.hpp>

namespace sand {

  class geomanager::subdetector_manager {

  public:
    subdetector_manager(const geomanager&, const path&);
    subdetector_manager(const subdetector_manager&) = delete;
    subdetector_manager& operator = (const subdetector_manager&) = delete;

    const pos_3d& centre() const { return m_centre; }

  protected:
    const geomanager& manager() { return r_mgr; }

  private:
    const geomanager& r_mgr;
    pos_3d m_centre;

  };

}
