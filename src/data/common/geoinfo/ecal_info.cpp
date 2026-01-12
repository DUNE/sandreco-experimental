#include <ecal_info.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>
#include <ufw/context.hpp>

#include <TGeoBoolNode.h>
#include <TGeoCompositeShape.h>
#include <TGeoTube.h>

namespace sand {

  namespace {
    void find_basic_element(TGeoShape* shape, TGeoHMatrix trasformation) {
      if (shape->IsComposite()) {
        auto c  = static_cast<TGeoCompositeShape*>(shape);
        auto u  = static_cast<TGeoUnion*>(c->GetBoolNode());
        auto l  = u->GetLeftShape();
        auto r  = u->GetRightShape();
        auto lt = static_cast<TGeoHMatrix*>(u->GetLeftMatrix());
        auto rt = static_cast<TGeoHMatrix*>(u->GetRightMatrix());
        find_basic_element(l, trasformation * (*lt));
        find_basic_element(r, trasformation * (*rt));
      } else {
        if (shape->TestShapeBit(TGeoShape::kGeoTubeSeg)) {
          auto tube = static_cast<TGeoTubeSeg*>(shape);

          UFW_INFO("      Found basic tube segment shape: {} (rmin={} rmax={} dz={} phi1={} phi2={})", shape->GetName(),
                   tube->GetRmin(), tube->GetRmax(), tube->GetDz(), tube->GetPhi1(), tube->GetPhi2());
        } else if (shape->TestShapeBit(TGeoShape::kGeoBox)) {
          UFW_INFO("      Found basic box shape: {}", shape->GetName());
        } else {
          UFW_INFO("      Found basic shape: {} (unknown type)", shape->GetName());
        }
      }
    }

    void find_basic_element(TGeoShape* shape) {
      TGeoHMatrix identity;
      find_basic_element(shape, identity);
    };
  } // namespace

  bool geoinfo::ecal_info::shape_element::face::are_points_coplanar() const {
    // Check coplanarity: four points are coplanar if the scalar triple product is zero
    // (p2-p1) · ((p3-p1) × (p4-p1)) = 0
    dir_3d u              = (p2 - p1).Unit();
    dir_3d w              = (p3 - p1).Unit();
    dir_3d z              = (p4 - p1).Unit();
    double triple_product = u.Dot(w.Cross(z));

    return is_zero_within_tolerance(triple_product);
  }

  geoinfo::ecal_info::shape_element::face::face(const pos_3d& v1, const pos_3d& v2, const pos_3d& v3, const pos_3d& v4)
    : p1(v1), p2(v2), p3(v3), p4(v4) {
    UFW_ASSERT(are_points_coplanar(), std::string("cell_face: four points are not coplanar"));

    normal_dir = normal();
  }

  dir_3d geoinfo::ecal_info::shape_element::face::normal() const {
    dir_3d u = p2 - p1;
    dir_3d w = p3 - p1;
    return u.Cross(w).Unit();
  }

  bool geoinfo::ecal_info::shape_element::are_faces_parallel() const {
    double cross_product_norm = face1.normal_dir.Cross(face2.normal_dir).R();
    return is_zero_within_tolerance(cross_product_norm);
  }

  bool geoinfo::ecal_info::shape_element::are_faces_perpendicular() const {
    double dot_product = face1.normal_dir.Dot(face2.normal_dir);
    return is_zero_within_tolerance(dot_product);
  }

  geoinfo::ecal_info::shape_element::shape_element(const face& f1, const face& f2) : face1(f1), face2(f2) {
    if (are_faces_parallel()) {
      type = shape_element_type::straight;
    } else if (are_faces_perpendicular()) {
      type = shape_element_type::curved;
    } else {
      UFW_EXCEPT(std::invalid_argument, "cell_element: faces normals are neither parallel nor perpendicular");
    }
  }

  geoinfo::ecal_info::ecal_info(const geoinfo& gi) : subdetector_info(gi, "kloe_calo_volume_PV_0") {
    UFW_INFO("ecal_info: constructed ECAL geoinfo");

    auto& tgm = ufw::context::current()->instance<root_tgeomanager>();
    auto nav  = tgm.navigator();

    auto ecal_path = gi.root_path() / path();
    nav->cd(ecal_path);
    auto calo = nav->get_node();

    for (int i = 0; i < calo->GetNdaughters(); ++i) {
      auto module = calo->GetDaughter(i);
      if (string_begins_with(module->GetName(), "ECAL_endcap_lv_PV_")) {
        auto endcap = module;
        for (int j = 0; j < endcap->GetNdaughters(); ++j) {
          module = endcap->GetDaughter(j);

          find_basic_element(module->GetVolume()->GetShape());

          // UFW_INFO(fmt::format("    Endcap Module {}: {}", j, module->GetName()));

          // Process endcap modules here
          // TGeoBoolNode Class for Endcap volumes

          // auto s       = static_cast<TGeoCompositeShape*>(module->GetVolume()->GetShape());
          // auto union1  = static_cast<TGeoUnion*>(s->GetBoolNode());
          // auto s_left  = union1->GetLeftShape();
          // auto s_right = union1->GetRightShape();

          // UFW_INFO(
          //     fmt::format("      Shape Left: {} {}", s_left->GetName(), s_left->IsComposite() ? "(composite)" :
          //     ""));
          // UFW_INFO(
          //     fmt::format("      Shape Right: {} {}", s_right->GetName(), s_right->IsComposite() ? "(composite)" :
          //     ""));
        }
      } else if (string_begins_with(module->GetName(), "ECAL_lv_PV_")) {
        UFW_INFO(fmt::format("    Barrel Module {}: {}", i, module->GetName()));

        // Process barrel modules here

        auto s = module->GetVolume()->GetShape();
      }
    }
  }

  geoinfo::ecal_info::~ecal_info() = default;

  geo_id geoinfo::ecal_info::id(const geo_path&) const {
    geo_id gi;
    return gi;
  }

  geo_path geoinfo::ecal_info::path(geo_id) const {
    geo_path gp;
    return gp;
  }

} // namespace sand
