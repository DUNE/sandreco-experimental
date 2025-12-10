#include <geant_gdml_parser/geant_gdml_parser.hpp>
#include <grain_info.hpp>
#include <ufw/context.hpp>

#include <G4MultiUnion.hh>
#include <G4SubtractionSolid.hh>
#include <G4VisExtent.hh>

namespace sand {

  namespace {

    const G4VPhysicalVolume* find_by_name(const G4VPhysicalVolume* pv, std::string_view name) {
      auto lv = pv->GetLogicalVolume();
      for (int i = 0; i != lv->GetNoDaughters(); ++i) {
        if (lv->GetDaughter(i)->GetName() == name) {
          return lv->GetDaughter(i);
        }
      }
      UFW_ERROR("No volume named '{}' was found.", name);
    }

    grain::pixel_array<geoinfo::grain_info::rect_f> parse_pixels(const G4VPhysicalVolume* sipms,
                                                                 const G4GDMLAuxMapType* auxmap) {
      grain::pixel_array<geoinfo::grain_info::rect_f> pixels;
      pos_3d centre(sipms->GetObjectTranslation());
      auto sipms_lv  = sipms->GetLogicalVolume();
      auto auxlist   = auxmap->find(sipms_lv)->second;
      auxlist        = *auxlist.at(0).auxList;
      auto cellcount = std::atoi(auxlist.at(0).value);
      auto cellsize  = std::atof(auxlist.at(1).value);
      auto celledge  = std::atof(auxlist.at(2).value);
      auto box       = dynamic_cast<G4Box*>(sipms_lv->GetSolid());
      float sx       = box->GetXHalfLength();
      float sy       = box->GetYHalfLength();
      float y        = sy + centre.y();
      for (int i = 0; i != cellcount; ++i) {
        float x = -sx + centre.x();
        for (int j = 0; j != cellcount; ++j) {
          pixels[i][j].left   = x;
          pixels[i][j].top    = y;
          pixels[i][j].right  = x + cellsize;
          pixels[i][j].bottom = y - cellsize;
          x += cellsize + celledge;
        }
        y -= cellsize + celledge;
      }
      return pixels;
    }

    std::vector<geoinfo::grain_info::rect_f> parse_holes(const G4MultiUnion* multiunion) {
      auto n_holes = multiunion->GetNumberOfSolids();
      std::vector<geoinfo::grain_info::rect_f> holes;
      holes.reserve(n_holes);
      for (int i = 0; i != n_holes; ++i) {
        auto box  = dynamic_cast<G4Box*>(multiunion->GetSolid(i));
        float sx  = box->GetXHalfLength();
        float sy  = box->GetYHalfLength();
        auto xfrm = multiunion->GetTransformation(i);
        geoinfo::grain_info::rect_f r{float(xfrm.dy()) - sy, float(xfrm.dx()) - sx, float(xfrm.dy()) + sy,
                                      float(xfrm.dx()) + sx};
        holes.push_back(r);
      }
      return holes;
    }

  } // namespace

  static constexpr char s_grain_path[] = "sand_inner_volume_PV_0/GRAIN_lv_PV_0/GRAIN_LAr_lv_PV_0";

  geoinfo::grain_info::grain_info(const geoinfo& gi, const std::string& inner_geom)
    : subdetector_info(gi, s_grain_path), m_fiducial_solid(nullptr) {
    UFW_INFO("Reading grain geometry details from {}.", inner_geom);
    // Parsing of this geometry assumes the file is well formed and complete
    auto& gdml             = ufw::context::current()->instance<grain::geant_gdml_parser>(ufw::public_id(inner_geom));
    auto world             = gdml.GetWorldVolume();
    auto cryostat_physical = find_by_name(world, "cryostat_physical");
    auto lar_physical      = find_by_name(cryostat_physical, "lar_volume_physical");
    auto lar_logical       = lar_physical->GetLogicalVolume();
    auto lar_extent        = lar_logical->GetSolid()->GetExtent();
    m_LAr_aabb.SetX(lar_extent.GetXmax());
    m_LAr_aabb.SetY(lar_extent.GetYmax());
    m_LAr_aabb.SetZ(lar_extent.GetZmax());
    for (const auto& [vol, list] : *gdml.GetAuxMap()) {
      for (const auto& item : list) {
        if (item.type == "Fiducial") {
          m_fiducial_solid = vol->GetSolid();
          auto ext         = m_fiducial_solid->GetExtent();
          m_fiducial_aabb.SetX(ext.GetXmax());
          m_fiducial_aabb.SetY(ext.GetYmax());
          m_fiducial_aabb.SetZ(ext.GetZmax());
        }
      }
    }
    if (!m_fiducial_solid) {
      UFW_ERROR("Geometry description does not contain a fiducial volume");
    }
    m_mask_cameras.reserve(lar_logical->GetNoDaughters());
    m_lens_cameras.reserve(lar_logical->GetNoDaughters());
    for (int i = 0; i != lar_logical->GetNoDaughters(); ++i) {
      auto camera = lar_logical->GetDaughter(i);
      if (camera->GetLogicalVolume()->GetName() == "cam_volume_mask") {
        add_camera_mask(camera, gdml);
      } else if (camera->GetLogicalVolume()->GetName() == "cam_volume_lens") {
        add_camera_lens(camera, gdml);
      } else {
        continue;
      }
    }
  }

  geoinfo::grain_info::~grain_info() = default;

  void geoinfo::grain_info::add_camera_mask(G4VPhysicalVolume* camera, G4GDMLParser& gdml) {
    auto rot  = camera->GetObjectRotationValue(); // GetObjectRotation is not reentrant (!)
    auto tran = camera->GetObjectTranslation();
    UFW_DEBUG("Camera '{}' (PV) found at [{:.3f}, {:.3f}, {:.3f}] with rotation matrix:", camera->GetName(), tran.x(),
              tran.y(), tran.z());
    UFW_DEBUG("[[{:.3f}, {:.3f}, {:.3f}], [{:.3f}, {:.3f}, {:.3f}], [{:.3f}, {:.3f}, {:.3f}]]", rot[0][0], rot[0][1],
              rot[0][2], rot[1][0], rot[1][1], rot[1][2], rot[2][0], rot[2][1], rot[2][2]);
    xform_3d loc2grain(rot.xx(), rot.xy(), rot.xz(), tran.x(), rot.yx(), rot.yy(), rot.yz(), tran.y(), rot.zx(),
                       rot.zy(), rot.zz(), tran.z());
    auto sipms      = find_by_name(camera, "photoDetector");
    auto mask       = find_by_name(camera, "cameraAssembly_mask");
    auto mask_lv    = mask->GetLogicalVolume();
    auto subsolid   = dynamic_cast<G4SubtractionSolid*>(mask_lv->GetSolid());
    auto mask_front = dynamic_cast<G4Box*>(subsolid->GetConstituentSolid(0));
    auto displaced  = dynamic_cast<G4DisplacedSolid*>(subsolid->GetConstituentSolid(1));
    auto multiunion = dynamic_cast<G4MultiUnion*>(displaced->GetConstituentMovedSolid());
    float sx        = mask_front->GetXHalfLength();
    float sy        = mask_front->GetYHalfLength();
    // id as size of vector, we need to parse it from the camera name
    mask_camera mc{camera->GetName(),
                   m_mask_cameras.size(),
                   uint8_t(grain::mask),
                   loc2grain,
                   parse_pixels(sipms, gdml.GetAuxMap()),
                   sipms->GetObjectTranslation().z(),
                   mask->GetObjectTranslation().z(),
                   rect_f{-sy, -sx, sy, sx},
                   parse_holes(multiunion)};
    m_mask_cameras.emplace_back(mc);
  }

  void geoinfo::grain_info::add_camera_lens(G4VPhysicalVolume* camera, G4GDMLParser& gdml) {
    auto rot  = camera->GetObjectRotationValue(); // GetObjectRotation is not reentrant (!)
    auto tran = camera->GetObjectTranslation();
    UFW_DEBUG("Camera '{}' (PV) found at [{:.3f}, {:.3f}, {:.3f}] with rotation matrix:", camera->GetName(), tran.x(),
              tran.y(), tran.z());
    UFW_DEBUG("[[{:.3f}, {:.3f}, {:.3f}], [{:.3f}, {:.3f}, {:.3f}], [{:.3f}, {:.3f}, {:.3f}]]", rot[0][0], rot[0][1],
              rot[0][2], rot[1][0], rot[1][1], rot[1][2], rot[2][0], rot[2][1], rot[2][2]);
    xform_3d loc2grain(rot.xx(), rot.xy(), rot.xz(), tran.x(), rot.yx(), rot.yy(), rot.yz(), tran.y(), rot.zx(),
                       rot.zy(), rot.zz(), tran.z());
    auto sipms = find_by_name(camera, "photoDetector_physical");
    auto lens  = find_by_name(camera, "gasLens_pv");
    // id as size of vector, we need to parse it from the camera name
    lens_camera lc{camera->GetName(),
                   m_lens_cameras.size(),
                   uint8_t(grain::lens),
                   loc2grain,
                   parse_pixels(sipms, gdml.GetAuxMap()),
                   sipms->GetObjectTranslation().z(),
                   lens->GetObjectTranslation().z()};
    m_lens_cameras.emplace_back(lc);
  }

  geo_id geoinfo::grain_info::id(const geo_path& gp) const {
    geo_id gi;
    if (gp == path()) {
      gi.subdetector = GRAIN;
    } else {
      UFW_ERROR("Incorrect path for GRAIN, expected '{}', got '{}'.", path(), gp);
    }
    return gi;
  }

  geo_path geoinfo::grain_info::path(geo_id gi) const {
    UFW_ASSERT(gi.subdetector == GRAIN, "Subdetector must be GRAIN");
    return path();
  }

  const geoinfo::grain_info::camera& geoinfo::grain_info::at(channel_id::link_t id) const {
    auto comp = [id](auto cam) { return cam.id == id; };
    auto it   = std::find_if(m_lens_cameras.begin(), m_lens_cameras.end(), comp);
    if (it != m_lens_cameras.end()) {
      return *it;
    }
    auto it2 = std::find_if(m_mask_cameras.begin(), m_mask_cameras.end(), comp);
    if (it2 != m_mask_cameras.end()) {
      return *it2;
    }
    UFW_ERROR("No camera of any type found with id = {}.", int(id));
  }

  const geoinfo::grain_info::camera& geoinfo::grain_info::at(const std::string& name) const {
    auto comp = [name](auto cam) { return cam.name == name; };
    auto it   = std::find_if(m_lens_cameras.begin(), m_lens_cameras.end(), comp);
    if (it != m_lens_cameras.end()) {
      return *it;
    }
    auto it2 = std::find_if(m_mask_cameras.begin(), m_mask_cameras.end(), comp);
    if (it2 != m_mask_cameras.end()) {
      return *it2;
    }
    UFW_ERROR("No camera of any type found with name = '{}'.", name);
  }

  /**
   * Creates a voxel grid with nonzero value when a given voxel is contained (even partially) in the fiducial.
   * Voxels are arranged such that, if the number of voxels in one axis is odd, the middle is centered on zero;
   * if the number is even, the boundary is at zero.
   * This function treats each axis separately, voxels can be non-cubical.
   */
  grain::voxel_array<uint8_t> geoinfo::grain_info::fiducial_voxels(dir_3d pitch) const {
    grain::size_3d count(std::ceil(2. * m_fiducial_aabb.x() / pitch.x()),
                         std::ceil(2. * m_fiducial_aabb.y() / pitch.y()),
                         std::ceil(2. * m_fiducial_aabb.z() / pitch.z()));
    grain::voxel_array<uint8_t> mask(count);
    dir_3d offset(count.x() / -2. * pitch.x(), count.y() / -2. * pitch.y(), count.z() / -2. * pitch.z());
    // super pedantic implementation, checks each vertex
    mask.for_each([=](grain::index_3d idx, uint8_t& value) {
      value = 0;
      for (int corner = 0; corner != 8; ++corner) {
        G4ThreeVector p(offset.x() + (idx.x() + (corner >> 2)) * pitch.x(),
                        offset.y() + (idx.y() + (corner >> 1 & 1)) * pitch.y(),
                        offset.z() + (idx.z() + (corner & 1)) * pitch.z());
        if (m_fiducial_solid->Inside(p) != kOutside) {
          value = 1;
          break;
        }
      }
    });
    return mask;
  }

  /**
   * Returns voxel center position in the local reference frame given the 3D index.
   * Assuming that voxels are arranged such that, if the number of voxels in one axis is odd, the middle is centered on zero.
   * if the number is even, the boundary is at zero.
   */
  pos_3d geoinfo::grain_info::voxel_index_to_position(grain::index_3d index, dir_3d pitch, grain::size_3d size) const {
    return pos_3d((index.x() + 0.5 - static_cast<float>(size.x()) / 2.)*pitch.x(), (index.y() + 0.5 - static_cast<float>(size.y()) / 2.)*pitch.y(), (index.z() + 0.5 - static_cast<float>(size.z()) / 2.)*pitch.z());
  }

} // namespace sand
