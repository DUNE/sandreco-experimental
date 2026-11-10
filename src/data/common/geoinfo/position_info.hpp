#pragma once

#include <common/sand.h>

#include <ufw/config.hpp>

namespace sand {

  class position_info {
   public:

   //position_info();
    virtual ~position_info();

    //The point is contaned? (needed?) (which subdetector?)
    virtual bool contains(const pos_3d& p) const = 0;

    virtual std::string material_at(const pos_3d& p) const;

    virtual std::string volume_at(const pos_3d& p) const;

   private:
    static std::string generic_material_at(const pos_3d& p);
    static std::string generic_volume_at(const pos_3d& p);
  };

} // namespace sand