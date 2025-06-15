#pragma once

#include <geoinfo/subdetector_info.hpp>

namespace sand {

  class geoinfo::ecal_info : public subdetector_info {

  public:
    enum class cell_type {
      barrel, endcap
    };

    struct cell {
      geo_id id;
      pos_3d centre;
      double length;
      cell_type type;
    };

  public:
    ecal_info(const geoinfo&);

    virtual ~ecal_info();

    const cell& at(pos_3d);

    const cell& at(geo_id);

    geo_id id(const geo_path&) const override;

    geo_path path(geo_id) const override;

  private:
    std::map<geo_id, cell> m_cells;

  };

}
