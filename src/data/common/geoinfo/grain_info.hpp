#pragma once

#include <algorithm>

#include <geoinfo/subdetector_info.hpp>

#include <grain/grain.h>

namespace sand {

  class geoinfo::grain_info : public subdetector_info {

  public:
    struct rect_f { //order and type matches volumereco, but should probably be changed to TLBR
      float bottom;
      float left;
      float top;
      float right;
    };

    struct camera {
      uint8_t id;
      uint8_t optics;
      uint8_t padding___[6];
      xform_3d cam_xform; //direction and orientation tbd
      grain::pixel_array<rect_f> sipm_active_areas; //plane tbd
      double sipm_offset;
    };

    struct lens_camera : public camera {

    };

    struct mask_camera : public camera {
      double mask_offset;
      rect_f box_perimeter;
      std::array<rect_f, grain::camera_width * grain::camera_height / 2> holes;
    };

  public:
    grain_info(const geoinfo&, const std::string&);

    template <typename Camera>
    std::enable_if_t<std::is_base_of_v<camera, Camera>, const Camera&> at(uint8_t);

  private:
    std::vector<lens_camera> m_lens_cameras;
    std::vector<mask_camera> m_mask_cameras;

  };

  template <typename Camera>
  std::enable_if_t<std::is_base_of_v<geoinfo::grain_info::camera, Camera>, const Camera&> geoinfo::grain_info::at(uint8_t id) {
    auto comp = [](auto cam, auto id){ return cam.id < id; };
    if constexpr (std::is_same_v<Camera, lens_camera>) {
      auto it = std::lower_bound(m_lens_cameras.begin(), m_lens_cameras.end(), id, comp);
      if (it != m_lens_cameras.end() && it->id == id) {
        return *it;
      }
    } else {
      auto it = std::lower_bound(m_mask_cameras.begin(), m_mask_cameras.end(), id, comp);
      if (it != m_mask_cameras.end() && it->id == id) {
        return *it;
      }
    }
    UFW_ERROR("No camera of the required type found with id = {}.", int(id));
  }

}
