#include <ecal_info.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>
#include <ufw/context.hpp>

#include <TGeoBoolNode.h>
#include <TGeoCompositeShape.h>
#include <TGeoTrd2.h>
#include <TGeoTube.h>
#include <TMath.h>

namespace sand {

  //////////////////////////////////////////////////////
  // stuff
  //////////////////////////////////////////////////////

  namespace {

    xform_3d to_xform_3d(const TGeoHMatrix& mat) {
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

        } else if (shape->TestShapeBit(TGeoShape::kGeoBox)) {
          auto transf = to_xform_3d(geo_transf);
          auto el     = box_to_shape_element(static_cast<TGeoBBox*>(shape));

          el.transform(transf);
          mod.elements.push_back(el);

        } else {
          UFW_ERROR(fmt::format("Unexpected shape: {}", shape->GetName()));
        }
      }
    }

    geoinfo::ecal_info::module get_module(TGeoShape* shape, TGeoHMatrix transf) {
      UFW_DEBUG("Processing ECAL module ==================================================");
      geoinfo::ecal_info::module mod;
      process_element(shape, transf, mod);
      UFW_DEBUG("=========================================================================");
      return mod;
    };
  } // namespace

  //////////////////////////////////////////////////////
  // geoinfo::ecal_info::shape_element_face
  //////////////////////////////////////////////////////

  bool geoinfo::ecal_info::shape_element_face::are_points_coplanar() const {
    // Check coplanarity: four points are coplanar if the scalar triple product is zero
    // (p2-p1) · ((p3-p1) × (p4-p1)) = 0
    dir_3d u              = (at(1) - at(0)).Unit();
    dir_3d w              = (at(2) - at(0)).Unit();
    dir_3d z              = (at(3) - at(0)).Unit();
    double triple_product = u.Dot(w.Cross(z));

    return is_zero_within_tolerance(triple_product);
  }

  geoinfo::ecal_info::shape_element_face::shape_element_face(const pos_3d& p1, const pos_3d& p2, const pos_3d& p3,
                                                             const pos_3d& p4)
    : v_{p1, p2, p3, p4} {
    UFW_ASSERT(are_points_coplanar(), std::string("cell_face: four points are not coplanar"));

    centroid_ = std::reduce(v_.begin(), v_.end(), pos_3d(), [&](const pos_3d& left, const pos_3d& right) {
      return pos_3d(left.x() + right.x(), left.y() + right.y(), left.z() + right.z()) / v_.size();
    });

    dir_3d u = at(1) - at(0);
    dir_3d w = at(2) - at(0);
    normal_  = u.Cross(w).Unit();
  }

  bool geoinfo::ecal_info::shape_element_face::operator== (const shape_element_face& other) const {
    auto mycopy_for = other;
    auto mycopy_rev = other;
    std::reverse(mycopy_rev.v_.begin(), mycopy_rev.v_.end());

    auto static same_elements = [](const shape_element_face& lhs, const shape_element_face& rhs) {
      for (auto const& pair : boost::combine(lhs.v_, rhs.v_)) {
        pos_3d p1, p2;
        boost::tie(p1, p2) = pair;
        if (!is_zero_within_tolerance((p1 - p2).R()))
          return false;
      }
      return true;
    };

    for (int i = 0; i < v_.size(); ++i) {
      if (same_elements(*this, mycopy_for) || same_elements(*this, mycopy_rev))
        return true;
      std::rotate(mycopy_for.v_.begin(), mycopy_for.v_.begin() + 1, mycopy_for.v_.end());
      std::rotate(mycopy_rev.v_.begin(), mycopy_rev.v_.begin() + 1, mycopy_rev.v_.end());
    }
    return false;
  }

  geoinfo::ecal_info::shape_element_face geoinfo::ecal_info::shape_element_face::transform(const xform_3d& transf) {
    std::for_each(v_.begin(), v_.end(), [&](pos_3d& p) { transf* p; });
    centroid_ = transf * centroid_;
    normal_   = transf * normal_;
    return *this;
  }

  //////////////////////////////////////////////////////
  // geoinfo::ecal_info::shape_element
  //////////////////////////////////////////////////////

  geoinfo::ecal_info::shape_element::shape_element(const shape_element_face& f1, const shape_element_face& f2)
    : face1(f1), face2(f2) {
    if (face1.is_straight(face2)) {
      type = shape_element_type::straight;
    } else if (face1.is_perpendicular_to(face2)) {
      type = shape_element_type::curved;
    } else {
      UFW_EXCEPT(std::invalid_argument, "cell_element: faces normals are neither parallel nor perpendicular");
    }
  }

  void geoinfo::ecal_info::shape_element::transform(const xform_3d& transf) {
    face1.transform(transf);
    face2.transform(transf);
  };

  pos_3d geoinfo::ecal_info::shape_element::axis_pos() const {
    // this has to be implemented
    pos_3d p;
    if (type == shape_element_type::curved) {
    } else {
    }
    return p;
  };

  dir_3d geoinfo::ecal_info::shape_element::axis_dir() const {
    // this has to be implemented
    if (type == shape_element_type::curved) {
      return face1 * face2;
    } else if (type == shape_element_type::straight) {
    } else {
      // UFW_EXCEPT(std::);
    }
    return dir_3d();
  };

  double geoinfo::ecal_info::shape_element::length() const {
    UFW_ASSERT(type == shape_element_type::curved, "length function only for curved shape");
    return (face1.centroid() - face2.centroid()).R();
  };

  pos_3d geoinfo::ecal_info::shape_element::to_face(int face_id) const {
    // this has to be implemented so that you segment one face and project segmentation to the other
    pos_3d p;
    if (type == shape_element_type::straight) {
    } else {
    }
    return p;
  };

  //////////////////////////////////////////////////////
  // geoinfo::ecal_info
  //////////////////////////////////////////////////////

  geoinfo::ecal_info::ecal_info(const geoinfo& gi) : subdetector_info(gi, "kloe_calo_volume_PV_0") {
    UFW_INFO("ecal_info: constructed ECAL geoinfo");

    ///////////////////////////////
    find_pattern(gi.root_path() / path());
    ///////////////////////////////
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

  void geoinfo::ecal_info::find_pattern(const geo_path& path) {
    std::smatch m;
    if (regex_search(path, m, re_ecal_barrel_module)) {
      barrel_module_cells(path);
      return;
    } else if (regex_search(path, m, re_ecal_endcap_module)) {
      endcap_module_cells(path);
      return;
    } else {
      auto nav = ufw::context::current()->instance<root_tgeomanager>().navigator();
      nav->cd(path);
      nav->for_each_node([&](auto node) { find_pattern(path / std::string(node->GetName())); });
    }
  }

  void geoinfo::ecal_info::endcap_module_cells(const geo_path& path) {
    auto nav = ufw::context::current()->instance<root_tgeomanager>().navigator();
    nav->cd(path);
    module m;
    nav->for_each_node([&](auto node) {
      auto shape = node->GetVolume()->GetShape();
      nav->cd(path / std::string(node->GetName()));
      auto transf = to_xform_3d(nav->get_hmatrix());
      if (shape->TestShapeBit(TGeoShape::kGeoTubeSeg)) {
        auto el = tube_to_shape_element(static_cast<TGeoTubeSeg*>(shape));
        el.transform(transf);
        m.elements.push_back(el);
      } else if (shape->TestShapeBit(TGeoShape::kGeoBox)) {
        auto el = box_to_shape_element(static_cast<TGeoBBox*>(shape));
        el.transform(transf);
        m.elements.push_back(el);
      } else {
        UFW_ERROR(fmt::format("Unexpected shape: {}", shape->GetName()));
      }
    });
  }

  void geoinfo::ecal_info::barrel_module_cells(const geo_path& path) {
    module m;
    auto nav = ufw::context::current()->instance<root_tgeomanager>().navigator();
    nav->cd(path);
    auto shape      = nav->get_node()->GetVolume()->GetShape();
    auto geo_transf = nav->get_hmatrix();
    auto transf     = to_xform_3d(geo_transf);
    auto el         = trd2_to_shape_element(static_cast<TGeoTrd2*>(shape));
    el.transform(transf);
    m.elements.push_back(el);
  }

} // namespace sand
