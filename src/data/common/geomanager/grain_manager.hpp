#pragma once

#include <geomanager/geomanager.hpp>

namespace sand {

  class geomanager::grain_manager {

  public:
    grain_manager(const geomanager&);
    grain_manager(const grain_manager&) = delete;
    grain_manager& operator = (const grain_manager&) = delete;

    const pos_3d& centre() const { return m_centre; }

  private:
    const geomanager& r_mgr;
    pos_3d m_centre;

  };

}
