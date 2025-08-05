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
      /// Unique camera name
      std::string name;
      /// Unique camera id
      uint8_t id;
      /// Bitmask of sand::grain::optics_type
      uint8_t optics;
      /**
       * Transforms local to grain coordinates. All other coordinates are in the local system
       * Local has z as the camera axis, with the light coming from z+
       * The centre is in the middle of the camera volume, so the sipms will have a negative
       * z position, while mask/lens will be at positive z.
       */
      xform_3d transform;
      /// These rects are on the xy plane with z = z_sipm
      grain::pixel_array<rect_f> sipm_active_areas;
      double z_sipm;
    };

    struct lens_camera : public camera {
      double z_lens;
    };

    struct mask_camera : public camera {
      double z_mask;
      rect_f box_perimeter;
      std::vector<rect_f> holes;
    };

  public:
    grain_info(const geoinfo&, const std::string&);

    virtual ~grain_info();

    using subdetector_info::path;

    geo_id id(const geo_path&) const override;

    geo_path path(geo_id) const override;

    template <typename Camera>
    std::enable_if_t<std::is_base_of_v<camera, Camera>, const Camera&> at(uint8_t);

    std::vector<lens_camera> lens_cameras() const { return m_lens_cameras; }

    std::vector<mask_camera> mask_cameras() const { return m_mask_cameras; }

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
