#pragma once

#include <algorithm>

#include <geoinfo/subdetector_info.hpp>

#include <grain/grain.h>

class G4GDMLParser;
class G4VPhysicalVolume;
class G4VSolid;

namespace sand {

  class geoinfo::grain_info : public subdetector_info {
   public:
    struct rect_f { // order and type matches volumereco, but should probably be changed to TLBR
      float bottom;
      float left;
      float top;
      float right;
    };

    struct camera {
      /// Unique camera name
      std::string name;
      /// Unique camera id
      channel_id::link_t id;
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

    using size_3d = dir_3d;
    using size_3i = ROOT::Math::DisplacementVector3D<ROOT::Math::Cartesian3D<size_t>>;

   public:
    grain_info(const geoinfo&, const std::string&);

    virtual ~grain_info();

    using subdetector_info::path;

    geo_id id(const geo_path&) const override;

    geo_path path(geo_id) const override;

    const camera& at(channel_id::link_t) const;

    const camera& at(const std::string&) const;

    template <typename Camera>
    std::enable_if_t<std::is_base_of_v<camera, Camera>, const Camera&> at(channel_id::link_t);

    template <typename Camera>
    std::enable_if_t<std::is_base_of_v<camera, Camera>, const Camera&> at(const std::string&);

    std::vector<lens_camera> lens_cameras() const { return m_lens_cameras; }

    std::vector<mask_camera> mask_cameras() const { return m_mask_cameras; }

    dir_3d fiducial_bbox() const { return m_fiducial_aabb; }

    dir_3d LAr_bbox() const { return m_LAr_aabb; }

    grain::voxel_array<uint8_t> fiducial_voxels(dir_3d pitch) const;

    pos_3d voxel_index_to_position(grain::index_3d, dir_3d, grain::size_3d) const;

   private:
    void add_camera_mask(G4VPhysicalVolume*, G4GDMLParser&);
    void add_camera_lens(G4VPhysicalVolume*, G4GDMLParser&);

   private:
    std::vector<lens_camera> m_lens_cameras;
    std::vector<mask_camera> m_mask_cameras;
    dir_3d m_fiducial_aabb;
    dir_3d m_LAr_aabb;
    G4VSolid* m_fiducial_solid;
  };

  template <typename Camera>
  std::enable_if_t<std::is_base_of_v<geoinfo::grain_info::camera, Camera>, const Camera&>
  geoinfo::grain_info::at(channel_id::link_t id) {
    auto comp = [id](auto cam) { return cam.id == id; };
    if constexpr (std::is_same_v<Camera, lens_camera>) {
      auto it = std::find_if(m_lens_cameras.begin(), m_lens_cameras.end(), comp);
      if (it != m_lens_cameras.end()) {
        return *it;
      }
    } else {
      auto it = std::find_if(m_mask_cameras.begin(), m_mask_cameras.end(), comp);
      if (it != m_mask_cameras.end()) {
        return *it;
      }
    }
    UFW_ERROR("No camera of the required type found with id = {}.", int(id));
  }

  template <typename Camera>
  std::enable_if_t<std::is_base_of_v<geoinfo::grain_info::camera, Camera>, const Camera&>
  geoinfo::grain_info::at(const std::string& name) {
    auto comp = [name](auto cam) { return cam.name == name; };
    if constexpr (std::is_same_v<Camera, lens_camera>) {
      auto it = std::find_if(m_lens_cameras.begin(), m_lens_cameras.end(), comp);
      if (it != m_lens_cameras.end()) {
        return *it;
      }
    } else {
      auto it = std::find_if(m_mask_cameras.begin(), m_mask_cameras.end(), comp);
      if (it != m_mask_cameras.end()) {
        return *it;
      }
    }
    UFW_ERROR("No camera of the required type found with name = '{}'.", name);
  }

} // namespace sand
