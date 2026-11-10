#include <position_info.hpp>
#include <ufw/context.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>

namespace sand {

   std::string position_info::material_at(const pos_3d& p) const {
    root_tgeomanager& tgm = ufw::context::current()->instance<root_tgeomanager>();
    auto nav  = tgm.navigator();

    TGeoNode* node = nav->FindNode(p.x(), p.y(), p.z());
    if (node == nullptr) {
      return "OUTSIDE";
    }

    TGeoVolume* vol = node->GetVolume();
    if (vol == nullptr) {
      return "NO_VOLUME";
    }

    TGeoMedium* med = vol->GetMedium();
    if (med == nullptr) {
      return "NO_MEDIUM";
    }

    TGeoMaterial* mat = med->GetMaterial();
    if (mat == nullptr) {
      return "NO_MATERIAL";
    }

    return mat->GetName();
  }


    std::string position_info::volume_at(const pos_3d& p) const {
    root_tgeomanager& tgm = ufw::context::current()->instance<root_tgeomanager>();
    auto nav  = tgm.navigator();

    TGeoNode* node = nav->FindNode(p.x(), p.y(), p.z());
    if (node == nullptr) {
      return "OUTSIDE";
    }

    TGeoVolume* vol = node->GetVolume();
    if (vol == nullptr) {
      return "NO_VOLUME";
    }

    return vol->GetName();
  }



  //std::string position_info::generic_volume_at(const pos_3d& p) { /* idem, come volume_at attuale */ }

  std::string position_info::material_at(const pos_3d& p) const { return generic_material_at(p); }
  std::string position_info::volume_at(const pos_3d& p) const   { return generic_volume_at(p); }

} // namespace sand