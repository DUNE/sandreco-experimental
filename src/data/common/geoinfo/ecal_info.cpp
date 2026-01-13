#include <ecal_info.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>
#include <ufw/context.hpp>

#include <TGeoBoolNode.h>
#include <TGeoCompositeShape.h>
#include <TGeoTrd2.h>
#include <TGeoTube.h>

namespace sand {

  namespace {

    xform_3d to_xform_3d(TGeoHMatrix& mat) {
      const Double_t* r = mat.GetRotationMatrix();
      const Double_t* t = mat.GetTranslation();

      rot_3d rotation(r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8]);
      dir_3d translation(t[0], t[1], t[2]);

      return xform_3d(rotation, translation);
    }

    void process_element(TGeoShape* shape, TGeoHMatrix geo_transf, geoinfo::ecal_info::module& mod) {
      if (shape->IsComposite()) {
        auto c  = static_cast<TGeoCompositeShape*>(shape);
        auto u  = static_cast<TGeoUnion*>(c->GetBoolNode());
        auto l  = u->GetLeftShape();
        auto r  = u->GetRightShape();
        auto lt = static_cast<TGeoHMatrix*>(u->GetLeftMatrix());
        auto rt = static_cast<TGeoHMatrix*>(u->GetRightMatrix());
        process_element(l, geo_transf * (*lt), mod);
        process_element(r, geo_transf * (*rt), mod);
      } else {
        if (shape->TestShapeBit(TGeoShape::kGeoTubeSeg)) {
          auto tube = static_cast<TGeoTubeSeg*>(shape);
          auto rmin = tube->GetRmin();
          auto rmax = tube->GetRmax();
          auto dz   = tube->GetDz();
          auto phi1 = tube->GetPhi1();
          auto phi2 = tube->GetPhi2();

          auto transf = to_xform_3d(geo_transf);

          geoinfo::ecal_info::shape_element el(
              geoinfo::ecal_info::shape_element_face(
                  pos_3d(rmin * std::cos(phi1 * TMath::DegToRad()), rmin * std::sin(phi1 * TMath::DegToRad()), dz),
                  pos_3d(rmin * std::cos(phi1 * TMath::DegToRad()), rmin * std::sin(phi1 * TMath::DegToRad()), -dz),
                  pos_3d(rmax * std::cos(phi1 * TMath::DegToRad()), rmax * std::sin(phi1 * TMath::DegToRad()), dz),
                  pos_3d(rmax * std::cos(phi1 * TMath::DegToRad()), rmax * std::sin(phi1 * TMath::DegToRad()), -dz)),
              geoinfo::ecal_info::shape_element_face(
                  pos_3d(rmin * std::cos(phi2 * TMath::DegToRad()), rmin * std::sin(phi2 * TMath::DegToRad()), dz),
                  pos_3d(rmin * std::cos(phi2 * TMath::DegToRad()), rmin * std::sin(phi2 * TMath::DegToRad()), -dz),
                  pos_3d(rmax * std::cos(phi2 * TMath::DegToRad()), rmax * std::sin(phi2 * TMath::DegToRad()), dz),
                  pos_3d(rmax * std::cos(phi2 * TMath::DegToRad()), rmax * std::sin(phi2 * TMath::DegToRad()), -dz)));

          el.transform(transf);
          mod.elements.push_back(el);

          UFW_INFO("Found ECAL endcap module element: {} ({})", shape->GetName(),
                   el.type == geoinfo::ecal_info::shape_element_type::straight ? "straight" : "curved");
          UFW_INFO("  ** Face 1 **");
          UFW_INFO("  Point 1: {}", el.face1.p1);
          UFW_INFO("  Point 2: {}", el.face1.p2);
          UFW_INFO("  Point 3: {}", el.face1.p3);
          UFW_INFO("  Point 4: {}", el.face1.p4);
          UFW_INFO("  ** Face 2 **");
          UFW_INFO("  Point 1: {}", el.face2.p1);
          UFW_INFO("  Point 2: {}", el.face2.p2);
          UFW_INFO("  Point 3: {}", el.face2.p3);
          UFW_INFO("  Point 4: {}", el.face2.p4);

        } else if (shape->TestShapeBit(TGeoShape::kGeoBox)) {
          auto box = static_cast<TGeoBBox*>(shape);
          auto dx  = box->GetDX();
          auto dy  = box->GetDY();
          auto dz  = box->GetDZ();

          auto transf = to_xform_3d(geo_transf);

          geoinfo::ecal_info::shape_element el(
              geoinfo::ecal_info::shape_element_face(pos_3d(-dx, -dy, dz), pos_3d(-dx, -dy, -dz), pos_3d(dx, -dy, dz),
                                                     pos_3d(dx, -dy, -dz)),
              geoinfo::ecal_info::shape_element_face(pos_3d(-dx, dy, dz), pos_3d(-dx, dy, -dz), pos_3d(dx, dy, dz),
                                                     pos_3d(dx, dy, -dz)));

          el.transform(transf);
          mod.elements.push_back(el);

          UFW_INFO("Found ECAL endcap module element: {} ({})", shape->GetName(),
                   el.type == geoinfo::ecal_info::shape_element_type::straight ? "straight" : "curved");
          UFW_INFO("  ** Face 1 **");
          UFW_INFO("  Point 1: {}", el.face1.p1);
          UFW_INFO("  Point 2: {}", el.face1.p2);
          UFW_INFO("  Point 3: {}", el.face1.p3);
          UFW_INFO("  Point 4: {}", el.face1.p4);
          UFW_INFO("  ** Face 2 **");
          UFW_INFO("  Point 1: {}", el.face2.p1);
          UFW_INFO("  Point 2: {}", el.face2.p2);
          UFW_INFO("  Point 3: {}", el.face2.p3);
          UFW_INFO("  Point 4: {}", el.face2.p4);

        } else {
          UFW_EXCEPT(std::invalid_argument, fmt::format("Unexpected shape: {}", shape->GetName()));
        }
      }
    }

    geoinfo::ecal_info::module get_module(TGeoShape* shape, TGeoHMatrix transf) {
      geoinfo::ecal_info::module mod;
      process_element(shape, transf, mod);
      return mod;
    };
  } // namespace

  bool geoinfo::ecal_info::shape_element_face::are_points_coplanar() const {
    // Check coplanarity: four points are coplanar if the scalar triple product is zero
    // (p2-p1) · ((p3-p1) × (p4-p1)) = 0
    dir_3d u              = (p2 - p1).Unit();
    dir_3d w              = (p3 - p1).Unit();
    dir_3d z              = (p4 - p1).Unit();
    double triple_product = u.Dot(w.Cross(z));

    return is_zero_within_tolerance(triple_product);
  }

  geoinfo::ecal_info::shape_element_face::shape_element_face(const pos_3d& v1, const pos_3d& v2, const pos_3d& v3,
                                                             const pos_3d& v4)
    : p1(v1), p2(v2), p3(v3), p4(v4) {
    UFW_ASSERT(are_points_coplanar(), std::string("cell_face: four points are not coplanar"));

    normal_dir = normal();
  }

  dir_3d geoinfo::ecal_info::shape_element_face::normal() const {
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

  geoinfo::ecal_info::shape_element::shape_element(const shape_element_face& f1, const shape_element_face& f2)
    : face1(f1), face2(f2) {
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

    nav->for_each_node([&](auto node) {
      if (string_begins_with(node->GetName(), "ECAL_endcap_lv_PV_")) {
        auto ecal_endcap_path = ecal_path / std::string(node->GetName());
        nav->cd(ecal_endcap_path);
        auto transf = nav->get_hmatrix();
        nav->for_each_node(
            [&](auto ecal_endcap_module) { get_module(ecal_endcap_module->GetVolume()->GetShape(), transf); });
      } else if (string_begins_with(node->GetName(), "ECAL_lv_PV_")) {
        auto ecal_barrel_module_path = ecal_path / std::string(node->GetName());
        nav->cd(ecal_barrel_module_path);
        auto geo_transf = nav->get_hmatrix();
        auto trd        = static_cast<TGeoTrd2*>(nav->get_node()->GetVolume()->GetShape());
        auto dx1        = trd->GetDx1();
        auto dx2        = trd->GetDx2();
        auto dy1        = trd->GetDy1();
        auto dy2        = trd->GetDy2();
        auto dz         = trd->GetDz();

        auto transf = to_xform_3d(geo_transf);

        geoinfo::ecal_info::shape_element el(
            geoinfo::ecal_info::shape_element_face(pos_3d(-dx1, -dy1, -dz), pos_3d(dx1, -dy1, -dz),
                                                   pos_3d(-dx2, -dy1, dz), pos_3d(dx2, -dy1, dz)),
            geoinfo::ecal_info::shape_element_face(pos_3d(-dx1, dy1, -dz), pos_3d(dx1, dy1, -dz), pos_3d(-dx2, dy1, dz),
                                                   pos_3d(dx2, dy1, dz)));
        el.transform(transf);

        UFW_INFO("Found ECAL barrel module element: {} ({})", nav->get_node()->GetName(),
                 el.type == geoinfo::ecal_info::shape_element_type::straight ? "straight" : "curved");
        UFW_INFO("  ** Face 1 **");
        UFW_INFO("  Point 1: {}", el.face1.p1);
        UFW_INFO("  Point 2: {}", el.face1.p2);
        UFW_INFO("  Point 3: {}", el.face1.p3);
        UFW_INFO("  Point 4: {}", el.face1.p4);
        UFW_INFO("  ** Face 2 **");
        UFW_INFO("  Point 1: {}", el.face2.p1);
        UFW_INFO("  Point 2: {}", el.face2.p2);
        UFW_INFO("  Point 3: {}", el.face2.p3);
        UFW_INFO("  Point 4: {}", el.face2.p4);
      } else {
        UFW_EXCEPT(std::invalid_argument, fmt::format("Unexpected node: {}", node->GetName()));
      }
    });
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
