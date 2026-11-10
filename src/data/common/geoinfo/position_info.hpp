#pragma once

#include <common/sand.h>

#include <memory>
#include <ufw/config.hpp>
#include <geoinfo/geoinfo.hpp>

#include <TGeoNode.h>

namespace sand {

  class geoinfo::position_info {
   public:
    position_info(pos_3d p, std::shared_ptr<const subdetector_info> detector, TGeoNode* node)
      : m_position(p), m_detector(detector), m_node(node) {};
    ~position_info();

    TGeoMaterial* get_material() {
      TGeoVolume* vol = m_node->GetVolume();
      if (vol == nullptr) {
        UFW_ERROR("This node at {} does not have a volume", m_position);
      }

      TGeoMedium* med = vol->GetMedium();
      if (med == nullptr) {
        UFW_ERROR("This node at {} does not have a medium", m_position);
      }

      TGeoMaterial* mat = med->GetMaterial();
      if (mat == nullptr) {
        UFW_ERROR("This node at {} does not have a material", m_position);
      }

      return mat;
    }

   private:
    pos_3d m_position;
    std::shared_ptr<const subdetector_info> m_detector;
    TGeoNode* m_node;
  };

} // namespace sand