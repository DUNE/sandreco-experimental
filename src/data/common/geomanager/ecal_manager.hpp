#pragma once

#include <geomanager/subdetector_manager.hpp>

namespace sand {

  class geomanager::ecal_manager : public subdetector_manager {

  public:
    enum class cell_type {
      barrel, endcap
    };

    struct cell {
      guid id;
      pos_3d centre;
      double length;
      cell_type type;
    };

  public:
    ecal_manager(const geomanager&);

    const cell& at(pos_3d);

    const cell& at(guid);

  private:
    std::map<guid, cell> m_cells;

  };

}
