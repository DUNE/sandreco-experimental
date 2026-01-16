#include <ecal_info.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>
#include <ufw/context.hpp>

#include <TGeoBoolNode.h>
#include <TGeoCompositeShape.h>
#include <TGeoTrd2.h>
#include <TGeoTube.h>
#include <TMath.h>

namespace sand {

  namespace {

    xform_3d to_xform_3d(TGeoHMatrix& mat) {
      const Double_t* r = mat.GetRotationMatrix();
      const Double_t* t = mat.GetTranslation();

      rot_3d rotation(r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8]);
      dir_3d translation(t[0], t[1], t[2]);

      return xform_3d(rotation, translation);
    }

    geoinfo::ecal_info::shape_element tube_to_shape_element(TGeoTubeSeg* tube) {
      auto rmin = tube->GetRmin();
      auto rmax = tube->GetRmax();
      auto dz   = tube->GetDz();
      auto phi1 = tube->GetPhi1();
      auto phi2 = tube->GetPhi2();

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

      return el;
    }

    geoinfo::ecal_info::shape_element box_to_shape_element(TGeoBBox* box) {
      auto dx = box->GetDX();
      auto dy = box->GetDY();
      auto dz = box->GetDZ();

      geoinfo::ecal_info::shape_element el(
          geoinfo::ecal_info::shape_element_face(pos_3d(-dx, -dy, dz), pos_3d(-dx, -dy, -dz), pos_3d(dx, -dy, dz),
                                                 pos_3d(dx, -dy, -dz)),
          geoinfo::ecal_info::shape_element_face(pos_3d(-dx, dy, dz), pos_3d(-dx, dy, -dz), pos_3d(dx, dy, dz),
                                                 pos_3d(dx, dy, -dz)));
      return el;
    }

    geoinfo::ecal_info::shape_element trd2_to_shape_element(TGeoTrd2* trd) {
      auto dx1 = trd->GetDx1();
      auto dx2 = trd->GetDx2();
      auto dy1 = trd->GetDy1();
      auto dy2 = trd->GetDy2();
      auto dz  = trd->GetDz();

      geoinfo::ecal_info::shape_element el(
          geoinfo::ecal_info::shape_element_face(pos_3d(-dx1, -dy1, -dz), pos_3d(dx1, -dy1, -dz),
                                                 pos_3d(-dx2, -dy1, dz), pos_3d(dx2, -dy1, dz)),
          geoinfo::ecal_info::shape_element_face(pos_3d(-dx1, dy1, -dz), pos_3d(dx1, dy1, -dz), pos_3d(-dx2, dy1, dz),
                                                 pos_3d(dx2, dy1, dz)));
      return el;
    }

    void print_shape_element_info(const geoinfo::ecal_info::shape_element& el, const TGeoShape* shape) {
      UFW_DEBUG("Found ECAL {} module element: {} ({})", shape->TestShapeBit(TGeoShape::kGeoTrd2) ? "barrel" : "endcap",
                shape->GetName(), el.type == geoinfo::ecal_info::shape_element_type::straight ? "straight" : "curved");
      UFW_DEBUG("  ** Face 1 **");
      UFW_DEBUG("  Point 1: {}", el.face1.p1);
      UFW_DEBUG("  Point 2: {}", el.face1.p2);
      UFW_DEBUG("  Point 3: {}", el.face1.p3);
      UFW_DEBUG("  Point 4: {}", el.face1.p4);
      UFW_DEBUG("  ** Face 2 **");
      UFW_DEBUG("  Point 1: {}", el.face2.p1);
      UFW_DEBUG("  Point 2: {}", el.face2.p2);
      UFW_DEBUG("  Point 3: {}", el.face2.p3);
      UFW_DEBUG("  Point 4: {}", el.face2.p4);
    }

    void print_hmatrix(TGeoHMatrix& mat) {
      const Double_t* r = mat.GetRotationMatrix();
      const Double_t* t = mat.GetTranslation();

      UFW_DEBUG("Rotation Matrix:");
      UFW_DEBUG("| {: .4f} {: .4f} {: .4f} |", r[0], r[1], r[2]);
      UFW_DEBUG("| {: .4f} {: .4f} {: .4f} |", r[3], r[4], r[5]);
      UFW_DEBUG("| {: .4f} {: .4f} {: .4f} |", r[6], r[7], r[8]);
      UFW_DEBUG("Translation Vector: ({: .4f}, {: .4f}, {: .4f})", t[0], t[1], t[2]);
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
          auto transf = to_xform_3d(geo_transf);
          auto el     = tube_to_shape_element(static_cast<TGeoTubeSeg*>(shape));

          el.transform(transf);
          mod.elements.push_back(el);
          print_shape_element_info(el, shape);

        } else if (shape->TestShapeBit(TGeoShape::kGeoBox)) {
          auto transf = to_xform_3d(geo_transf);
          auto el     = box_to_shape_element(static_cast<TGeoBBox*>(shape));

          el.transform(transf);
          mod.elements.push_back(el);
          print_shape_element_info(el, shape);

        } else {
          UFW_ERROR(fmt::format("Unexpected shape: {}", shape->GetName()));
        }
      }
    }

    geoinfo::ecal_info::module get_module(TGeoShape* shape, TGeoHMatrix transf) {
      UFW_DEBUG("Processing ECAL module ==================================================");
      print_hmatrix(transf);
      geoinfo::ecal_info::module mod;
      process_element(shape, transf, mod);
      UFW_DEBUG("=========================================================================");
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
      auto node_name = std::string(node->GetName());
      if (node_name.rfind("ECAL_endcap_lv_PV_") == 0) {
        auto ecal_endcap_path = ecal_path / node_name;
        nav->cd(ecal_endcap_path);
        nav->for_each_node([&](auto ecal_endcap_module) {
          auto ecal_endcap_module_path = ecal_endcap_path / std::string(ecal_endcap_module->GetName());
          nav->cd(ecal_endcap_module_path);
          auto transf = nav->get_hmatrix();
          get_module(ecal_endcap_module->GetVolume()->GetShape(), transf);
        });
      } else if (node_name.rfind("ECAL_lv_PV_") == 0) {
        auto ecal_barrel_module_path = ecal_path / node_name;
        nav->cd(ecal_barrel_module_path);
        auto geo_transf = nav->get_hmatrix();
        auto transf     = to_xform_3d(geo_transf);
        auto trd        = static_cast<TGeoTrd2*>(nav->get_node()->GetVolume()->GetShape());
        auto el         = trd2_to_shape_element(trd);
        el.transform(transf);
        print_shape_element_info(el, trd);
      } else {
        UFW_ERROR(fmt::format("Unexpected node: {}", node->GetName()));
      }
    });
  }

  geoinfo::ecal_info::~ecal_info() = default;

  geo_id geoinfo::ecal_info::id(const geo_path& path) const {
    // /volWorld_PV_1/rockBox_lv_PV_0/volDetEnclosure_PV_0/volSAND_PV_0/MagIntVol_volume_PV_0/kloe_calo_volume_PV_0/ECAL_lv_PV_0/volECALActiveSlab_0_PV_0
    // -> reegex:
    // \/volWorld_PV_1\/rockBox_lv_PV_0\/volDetEnclosure_PV_0\/volSAND_PV_0\/MagIntVol_volume_PV_0\/kloe_calo_volume_PV_0\/ECAL_lv_PV_(\d+)\/volECALActiveSlab_(\d+)_PV_0$
    // -> working:
    // "^\\/volWorld_PV_1\\/rockBox_lv_PV_0\\/volDetEnclosure_PV_0\\/volSAND_PV_0\\/MagIntVol_volume_PV_0\\/kloe_calo_volume_PV_0\\/ECAL_lv_PV_(\\d+)\\/volECALActiveSlab_(\\d+)_PV_0$"
    // active volume pth for barrel
    // /volWorld_PV_1/rockBox_lv_PV_0/volDetEnclosure_PV_0/volSAND_PV_0/MagIntVol_volume_PV_0/kloe_calo_volume_PV_0/ECAL_endcap_lv_PV_1/ECAL_ec_mod_0_lv_PV_0/ECAL_ec_mod_curv_0_lv_PV_1/endvolECALcurvActiveSlab_0_0_PV_0
    // root [57] n->GetDaughter(0)->GetName()
    // (const char *) "ECAL_ec_mod_vert_8_lv_PV_0"
    // root [58] n->GetDaughter(1)->GetName()
    // (const char *) "ECAL_ec_mod_curv_8_lv_PV_0"
    // root [59] n->GetDaughter(2)->GetName()
    // (const char *) "ECAL_ec_mod_curv_8_lv_PV_1"
    // root [60] n->GetDaughter(3)->GetName()
    // (const char *) "ECAL_ec_mod_hor_8_lv_PV_0"
    // root [61] n->GetDaughter(4)->GetName()
    // (const char *) "ECAL_ec_mod_hor_8_lv_PV_1"
    // kloe_calo_volume_PV_0/
    //   ECAL_endcap_lv_PV_0
    //   ECAL_endcap_lv_PV_1
    //     ECAL_ec_mod_0_lv_PV_0
    //     ECAL_ec_mod_0_lv_PV_1
    //     ECAL_ec_mod_1_lv_PV_0
    //     ...
    //     ECAL_ec_mod_15_lv_PV_1
    //       ECAL_ec_mod_vert_15_lv_PV_0
    //         endvolECALActiveSlab_15_PV_0
    //         endvolECALActiveSlab_15_PV_1
    //       ECAL_ec_mod_curv_15_lv_PV_0
    //         endvolECALcurvActiveSlab_15_0_PV_0
    //         endvolECALcurvActiveSlab_15_1_PV_0
    //       ECAL_ec_mod_curv_15_lv_PV_1
    //         ECAL_ec_mod_curv_Alplate_15_lv_PV_0
    //         endvolECALcurvActiveSlab_15_0_PV_0
    //         endvolECALcurvPassiveSlab_15_0_PV_0
    //         endvolECALcurvPassiveSlab_15_1_PV_0
    //         endvolECALcurvActiveSlab_15_208_PV_0
    //       ECAL_ec_mod_hor_15_lv_PV_0
    //         endvolECALstraightActiveSlab_15_PV_0
    //         endvolECALstraightActiveSlab_15_PV_1
    //       ECAL_ec_mod_hor_15_lv_PV_1
    geo_id gi;
    std::smatch m;
    if (regex_search(path, m, re_ecal_barrel_sensible_volume)) {
      gi.ecal.subdetector = sand::subdetector_t::ECAL;
      gi.ecal.region      = sand::geo_id::region_t::BARREL;
      gi.ecal.supermodule = static_cast<sand::geo_id::supermodule_t>(std::stoi(m[1]));
      gi.ecal.element     = sand::geo_id::element_t::NONE;
      gi.ecal.plane       = static_cast<sand::geo_id::plane_t>(std::stoi(m[2]));
    } else if (regex_search(path, m, re_ecal_endcap_sensible_volume)) {
      gi.ecal.subdetector = sand::subdetector_t::ECAL;
      gi.ecal.region      = (m[1] == "0") ? sand::geo_id::region_t::ENDCAP_A : sand::geo_id::region_t::ENDCAP_B;
      gi.ecal.supermodule = static_cast<sand::geo_id::supermodule_t>(std::stoi(m[2]) + std::stoi(m[3]) * 16);
      if (m[4] == "vert") {
        gi.ecal.element = sand::geo_id::element_t::ENDCAP_VERTICAL;
        gi.ecal.plane   = static_cast<sand::geo_id::plane_t>(std::stoi(m[11]));
      } else if (m[4] == "curv" && std::stoi(m[6]) == 0) {
        gi.ecal.plane   = static_cast<sand::geo_id::plane_t>(std::stoi(m[10]));
        gi.ecal.element = sand::geo_id::element_t::ENDCAP_CURVE_TOP;
      } else if (m[4] == "curv" && std::stoi(m[6]) == 1) {
        gi.ecal.plane   = static_cast<sand::geo_id::plane_t>(std::stoi(m[10]));
        gi.ecal.element = sand::geo_id::element_t::ENDCAP_CURVE_BOT;
      } else if (m[4] == "hor" && std::stoi(m[6]) == 0) {
        gi.ecal.plane   = static_cast<sand::geo_id::plane_t>(std::stoi(m[11]));
        gi.ecal.element = sand::geo_id::element_t::ENDCAP_HOR_TOP;
      } else if (m[4] == "hor" && std::stoi(m[6]) == 1) {
        gi.ecal.plane   = static_cast<sand::geo_id::plane_t>(std::stoi(m[11]));
        gi.ecal.element = sand::geo_id::element_t::ENDCAP_HOR_BOT;
      } else {
        UFW_ERROR(fmt::format("Unknown ECAL endcap element type: {}", m[4].str()));
      }
    } else {
      UFW_ERROR(fmt::format("Provided geo_path {} does not match ECAL sensible volume pattern", path));
    }
    return gi;
  }

  geo_path geoinfo::ecal_info::path(geo_id id) const {
    geo_path gp = path();
    if (id.ecal.region == sand::geo_id::region_t::BARREL) {
      gp /= fmt::format("ECAL_lv_PV_{}/volECALActiveSlab_{}_PV_0", id.ecal.supermodule, id.ecal.plane);
    } else if ((id.ecal.region == sand::geo_id::region_t::ENDCAP_A)
               || (id.ecal.region == sand::geo_id::region_t::ENDCAP_B)) {
      int index1 = (id.ecal.region == sand::geo_id::region_t::ENDCAP_A) ? 0 : 1;
      std::string str1;
      std::string str2;
      std::string str3;
      int index2;
      int index3;
      switch (id.ecal.element) {
      case sand::geo_id::element_t::ENDCAP_VERTICAL:
        str1   = "vert";
        str2   = "";
        str3   = "";
        index2 = 0;
        index3 = id.ecal.plane;
        break;
      case sand::geo_id::element_t::ENDCAP_CURVE_TOP:
        str1   = "curv";
        str2   = "curv";
        str3   = "_" + std::to_string(id.ecal.plane);
        index2 = 0;
        index3 = 0;
        break;
      case sand::geo_id::element_t::ENDCAP_CURVE_BOT:
        str1   = "curv";
        str2   = "curv";
        str3   = "_" + std::to_string(id.ecal.plane);
        index2 = 1;
        index3 = 0;
        break;
      case sand::geo_id::element_t::ENDCAP_HOR_TOP:
        str1   = "hor";
        str2   = "straight";
        str3   = "";
        index2 = 0;
        index3 = id.ecal.plane;
        break;
      case sand::geo_id::element_t::ENDCAP_HOR_BOT:
        str1   = "hor";
        str2   = "straight";
        str3   = "";
        index2 = 1;
        index3 = id.ecal.plane;
        break;
      default:
        UFW_ERROR("Invalid ECAL endcap element type");
      }
      gp /= fmt::format(
          "ECAL_endcap_lv_PV_{}/ECAL_ec_mod_{}_lv_PV_{}/ECAL_ec_mod_{}_{}_lv_PV_{}/endvolECAL{}ActiveSlab_{}_PV_{}",
          index1, id.ecal.supermodule % 16, id.ecal.supermodule / 16, str1, id.ecal.supermodule, index2, str2,
          id.ecal.supermodule, str3, index3);
    } else {
      UFW_ERROR("Invalid ECAL region type");
    }
    return gp;
  }

} // namespace sand
