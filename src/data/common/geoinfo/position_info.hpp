#pragma once

#include <common/sand.h>

#include <ufw/config.hpp>
#include <geoinfo/subdetector_info.hpp>

namespace sand {
  class geoinfo::position_info {
   public:
    position_info() {};
    virtual ~position_info();

   private:
    pos_3d m_position;
    std::shared_ptr<geoinfo::subdetector_info> m_detector;
  };

} // namespace sand