#include <ecal_info.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>
#include <ufw/context.hpp>
#include <boost/range/combine.hpp>

// #include <TGeoBoolNode.h>
// #include <TGeoCompositeShape.h>
#include <Math/AxisAngle.h>
// #include <Math/GenVector/3DConversions.h>
#include <TGeoTrd2.h>
#include <TGeoTube.h>
#include <TMath.h>

namespace sand {

  using rot_ang = ROOT::Math::AxisAngle;

  //////////////////////////////////////////////////////
  // stuff
  //////////////////////////////////////////////////////

  namespace {
    constexpr double ktolerance(1e-10);
    constexpr pos_3d orig(0., 0., 0.);
    inline bool is_zero_within_tolerance(double value) { return std::abs(value) < ktolerance; };

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

      auto to_shape_element_face = [&](double phi) {
        return geoinfo::ecal_info::shape_element_face(
            pos_3d(rmin * std::cos(phi * TMath::DegToRad()), rmin * std::sin(phi * TMath::DegToRad()), dz),
            pos_3d(rmin * std::cos(phi * TMath::DegToRad()), rmin * std::sin(phi * TMath::DegToRad()), -dz),
            pos_3d(rmax * std::cos(phi * TMath::DegToRad()), rmax * std::sin(phi * TMath::DegToRad()), -dz),
            pos_3d(rmax * std::cos(phi * TMath::DegToRad()), rmax * std::sin(phi * TMath::DegToRad()), dz));
      };
      return geoinfo::ecal_info::shape_element(to_shape_element_face(phi1), to_shape_element_face(phi2));
    }

    geoinfo::ecal_info::shape_element box_to_shape_element(TGeoBBox* box) {
      auto dx = box->GetDX();
      auto dy = box->GetDY();
      auto dz = box->GetDZ();

      return geoinfo::ecal_info::shape_element(
          geoinfo::ecal_info::shape_element_face(pos_3d(-dx, -dy, dz), pos_3d(-dx, -dy, -dz), pos_3d(dx, -dy, -dz),
                                                 pos_3d(dx, -dy, dz)),
          geoinfo::ecal_info::shape_element_face(pos_3d(-dx, dy, dz), pos_3d(-dx, dy, -dz), pos_3d(dx, dy, -dz),
                                                 pos_3d(dx, dy, dz)));
    }

    geoinfo::ecal_info::shape_element trd2_to_shape_element(TGeoTrd2* trd) {
      auto dx1 = trd->GetDx1();
      auto dx2 = trd->GetDx2();
      auto dy1 = trd->GetDy1();
      auto dy2 = trd->GetDy2();
      auto dz  = trd->GetDz();

      return geoinfo::ecal_info::shape_element(
          geoinfo::ecal_info::shape_element_face(pos_3d(-dx1, -dy1, -dz), pos_3d(dx1, -dy1, -dz), pos_3d(dx2, -dy1, dz),
                                                 pos_3d(-dx2, -dy1, dz)),
          geoinfo::ecal_info::shape_element_face(pos_3d(-dx1, dy1, -dz), pos_3d(dx1, dy1, -dz), pos_3d(dx2, dy1, dz),
                                                 pos_3d(-dx2, dy1, dz)));
    }

    // void process_element(TGeoShape* shape, TGeoHMatrix geo_transf, geoinfo::ecal_info::module& mod) {
    //   if (shape->IsComposite()) {
    //     auto c  = static_cast<TGeoCompositeShape*>(shape);
    //     auto u  = static_cast<TGeoUnion*>(c->GetBoolNode());
    //     auto l  = u->GetLeftShape();
    //     auto r  = u->GetRightShape();
    //     auto lt = static_cast<TGeoHMatrix*>(u->GetLeftMatrix());
    //     auto rt = static_cast<TGeoHMatrix*>(u->GetRightMatrix());
    //     process_element(l, geo_transf * (*lt), mod);
    //     process_element(r, geo_transf * (*rt), mod);
    //   } else {
    //     if (shape->TestShapeBit(TGeoShape::kGeoTubeSeg)) {
    //       auto transf = to_xform_3d(geo_transf);
    //       auto el     = tube_to_shape_element(static_cast<TGeoTubeSeg*>(shape));

    //       el.transform(transf);
    //       mod.elements.push_back(el);

    //     } else if (shape->TestShapeBit(TGeoShape::kGeoBox)) {
    //       auto transf = to_xform_3d(geo_transf);
    //       auto el     = box_to_shape_element(static_cast<TGeoBBox*>(shape));

    //       el.transform(transf);
    //       mod.elements.push_back(el);

    //     } else {
    //       UFW_ERROR(fmt::format("Unexpected shape: {}", shape->GetName()));
    //     }
    //   }
    // }

    // geoinfo::ecal_info::module get_module(TGeoShape* shape, TGeoHMatrix transf) {
    //   UFW_DEBUG("Processing ECAL module ==================================================");
    //   geoinfo::ecal_info::module mod;
    //   process_element(shape, transf, mod);
    //   UFW_DEBUG("=========================================================================");
    //   return mod;
    // };

    // to be removed
    // bool lines_intercept(const pos_3d& p1, const dir_3d& v1, const pos_3d& p2, const dir_3d& v2) {
    //   return is_zero_within_tolerance(v1.Cross(v2).Dot(p1 - p2));
    // }

    // to be removed
    // bool find_intercepting_sides(const geoinfo::ecal_info::shape_element_face& f1,
    //                              const geoinfo::ecal_info::shape_element_face& f2, size_t& idx1, size_t& idx2) {
    //   for (idx1 = 0; idx1 < 4; idx1++) {
    //     for (idx2 = 0; idx2 < 4; idx2++) {
    //       if (lines_intercept(f1.vtx(idx1), f1.side(idx1), f2.vtx(idx2), f2.side(idx2)))
    //         return true;
    //     }
    //   }
    //   return false;
    // }

    // to be removed
    // pos_3d intercepting_point(const pos_3d& p1, const dir_3d& v1, const pos_3d& p2, const dir_3d& v2) {
    //   dir_3d vp = v2.Cross(p1 - p2);
    //   dir_3d vv = v2.Cross(v1);

    //   int sign = vp.Dot(vv) > 0 ? +1 : -1;
    //   return p1 + sign * vp.R() / vv.R() * v1;
    // }

    inline dir_3d r_wrt_axis(const pos_3d& p, const pos_3d& l_pos, const dir_3d& l_dir) {
      return (p - l_pos) - (p - l_pos).Dot(l_dir) / l_dir.R() * l_dir;
    }

    inline pos_3d z_wrt_axis(const pos_3d& p, const pos_3d& l_pos, const dir_3d& l_dir) {
      return p + r_wrt_axis(p, l_pos, l_dir);
    }

    double ang_wrt_axis(const pos_3d& p1, const pos_3d& p2, const pos_3d& l_pos, const dir_3d& l_dir) {
      auto r1 = r_wrt_axis(p1, l_pos, l_dir);
      auto r2 = r_wrt_axis(p2, l_pos, l_dir);
      return TMath::ACos(r1.Dot(r2) / (r1.R() * r2.R()));
    }

    void print_face(const geoinfo::ecal_info::shape_element_face& f) {
      UFW_DEBUG(fmt::format("Vertices:"));
      for (size_t i = 0; i < 4; i++)
        UFW_DEBUG(fmt::format(" {}- {}", i, f.vtx(i)));
      UFW_DEBUG(fmt::format("Normal  : {}", f.normal()));
      UFW_DEBUG(fmt::format("Centroid: {}", f.centroid()));
    }

    void print_element(const geoinfo::ecal_info::shape_element& el) {
      UFW_DEBUG(fmt::format("Element type: {}",
                            el.type() == geoinfo::ecal_info::shape_element_type::curved ? "curved" : "straight"));
      UFW_DEBUG(fmt::format("face #1: "));
      print_face(el.face1());
      UFW_DEBUG(fmt::format("face #2: "));
      print_face(el.face2());
      UFW_DEBUG(fmt::format("Axis Position : {}", el.axis_pos()));
      UFW_DEBUG(fmt::format("Axis Direction: {}", el.axis_dir()));
    }

  } // namespace

  //////////////////////////////////////////////////////
  // geoinfo::ecal_info::shape_element_face
  //////////////////////////////////////////////////////

  bool geoinfo::ecal_info::shape_element_face::are_points_coplanar() const {
    // Check coplanarity: four points are coplanar if the scalar triple product is zero
    // (p2-p1) · ((p3-p1) × (p4-p1)) = 0
    dir_3d u              = (vtx(1) - vtx(0)).Unit();
    dir_3d w              = (vtx(2) - vtx(0)).Unit();
    dir_3d z              = (vtx(3) - vtx(0)).Unit();
    double triple_product = u.Dot(w.Cross(z));

    return is_zero_within_tolerance(triple_product);
  }

  geoinfo::ecal_info::shape_element_face::shape_element_face(const pos_3d& p1, const pos_3d& p2, const pos_3d& p3,
                                                             const pos_3d& p4)
    : v_{p1, p2, p3, p4} {
    UFW_ASSERT(are_points_coplanar(), std::string("cell_face: four points are not coplanar"));

    centroid_ = vtx(0) + 0.25 * ((vtx(1) - vtx(0)) + (vtx(2) - vtx(0)) + (vtx(3) - vtx(0)));

    dir_3d u = vtx(1) - vtx(0);
    dir_3d w = vtx(2) - vtx(0);
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
    std::for_each(v_.begin(), v_.end(), [&](pos_3d& p) { p = transf * p; });
    centroid_ = transf * centroid();
    normal_   = transf * normal();
    return *this;
  }

  // bool geoinfo::ecal_info::shape_element_face::is_straight(shape_element_face other) const {
  //   return *this == other.transform(xform_3d(centroid_ - other.centroid_));
  // };

  // bool geoinfo::ecal_info::shape_element_face::is_curved(shape_element_face other) const {
  //   // find two side whose lines crosses each other (i.e. having a common point)

  //   auto& f1 = *this;
  //   auto& f2 = other;
  //   auto h1  = f1.normal().Dot(f1.centroid());
  //   auto h2  = f2.normal().Dot(f2.centroid());
  //   auto dpr = f1.normal().Dot(f2.normal());
  //   auto det = (1 - dpr * dpr);

  //   if (is_zero_within_tolerance(det))
  //     return false;

  //   auto c1       = (h1 - h2 * dpr) / det;
  //   auto c2       = (h2 - h1 * dpr) / det;
  //   auto axis_pos = c1 * f1.normal() + c2 * f2.normal();
  //   auto axis_dir = f1.normal().Cross(f2.normal());
  //   auto angle    = TMath::ACos(f1.normal().Dot(f2.normal()));

  //   if (!is_zero_within_tolerance(std::fabs(angle) - 0.5 * TMath::Pi()))
  //     return false;

  //   return f1 == f2.transform(xform_3d(rot_3d(rot_ang(axis_dir, -angle)), axis_pos));
  // };

  //////////////////////////////////////////////////////
  // geoinfo::ecal_info::shape_element
  //////////////////////////////////////////////////////

  geoinfo::ecal_info::shape_element::shape_element(const shape_element_face& f1, const shape_element_face& f2)
    : face1_(f1), face2_(f2) {
    if (!(is_straight() || is_curved())) {
      UFW_EXCEPT(std::invalid_argument, "cell_element: Neither a straight element nor a curved element!!");
    }
    // if (face1().is_straight(face2())) {
    //   type_     = shape_element_type::straight;
    //   axis_pos_ = face1().centroid();
    //   axis_dir_ = face2().centroid() - face1().centroid();
    // } else if (face1().is_curved(face2())) {
    //   type_          = shape_element_type::curved;
    //   auto h1        = face1().normal().Dot(face1().centroid());
    //   auto h2        = face2().normal().Dot(face2().centroid());
    //   auto dpr       = face1().normal().Dot(face2().normal());
    //   auto det       = (1 - dpr * dpr);
    //   auto c1        = (h1 - h2 * dpr) / det;
    //   auto c2        = (h2 - h1 * dpr) / det;
    //   auto axis_pos_ = c1 * f1.normal() + c2 * f2.normal();
    //   auto axis_dir_ = f1.normal().Cross(f2.normal());
    // } else {
    //   UFW_EXCEPT(std::invalid_argument, "cell_element: faces normals are neither parallel nor perpendicular");
    // }
  }

  void geoinfo::ecal_info::shape_element::transform(const xform_3d& transf) {
    face1_.transform(transf);
    face2_.transform(transf);
    axis_pos_ = transf * axis_pos();
    axis_dir_ = transf * axis_dir();
  };

  // pos_3d geoinfo::ecal_info::shape_element::axis_pos() const {
  //   // this has to be implemented
  //   pos_3d p;
  //   if (type == shape_element_type::curved) {
  //   } else {
  //   }
  //   return p;
  // };

  // dir_3d geoinfo::ecal_info::shape_element::axis_dir() const {
  //   // this has to be implemented
  //   if (type == shape_element_type::curved) {
  //     return face1 * face2;
  //   } else if (type == shape_element_type::straight) {
  //   } else {
  //     // UFW_EXCEPT(std::);
  //   }
  //   return dir_3d();
  // };

  // double geoinfo::ecal_info::shape_element::length() const {
  //   UFW_ASSERT(type == shape_element_type::curved, "length function only for curved shape");
  //   return (face1.centroid() - face2.centroid()).R();
  // };

  pos_3d geoinfo::ecal_info::shape_element::to_face_point(const pos_3d& p, size_t faceid) const {
    // this has to be implemented so that you segment one face and project segmentation to the other
    const shape_element_face* f;
    if (faceid == 1)
      f = &face1();
    else if (faceid == 2)
      f = &face2();
    else {
      UFW_ERROR(fmt::format("faceid can be either 1 or 2; provided value: {}", faceid));
      return pos_3d();
    }

    if (type() == shape_element_type::straight) {
      return p + f->normal().Dot(f->centroid() - p) / f->normal().Dot(axis_dir()) * axis_dir();
    } else {
      auto ang = ang_wrt_axis(p, f->centroid(), axis_pos(), axis_dir());
      auto trf = xform_3d(rot_3d(rot_ang(axis_dir(), -ang)), axis_pos() - orig);
      return trf * p;
    }
  };

  double geoinfo::ecal_info::shape_element::to_face_pathlength(const pos_3d& p1, size_t faceid) const {
    auto p2 = to_face_point(p1, faceid);
    if (type() == shape_element_type::straight) {
      if (!is_zero_within_tolerance(axis_dir().Cross(p1 - p2).R())) {
        UFW_ERROR(fmt::format("Points: {}, {} are not aligned along shape_element axis", p1, p2));
        return -1.;
      }
      return (p1 - p2).R();
    } else {
      auto r1 = r_wrt_axis(p1, axis_pos(), axis_dir());
      auto r2 = r_wrt_axis(p2, axis_pos(), axis_dir());
      auto z1 = z_wrt_axis(p1, axis_pos(), axis_dir());
      auto z2 = z_wrt_axis(p2, axis_pos(), axis_dir());
      if (!is_zero_within_tolerance(r1.R() - r2.R()) || !is_zero_within_tolerance((z1 - z2).R())) {
        UFW_ERROR(fmt::format("Points: {}, {} are not aligned along shape_element axis", p1, p2));
        return -1.;
      }
      auto ang = ang_wrt_axis(p1, p2, axis_pos(), axis_dir());
      return r1.R() * std::fabs(ang);
    }
  };

  bool geoinfo::ecal_info::shape_element::is_straight() {
    auto test_face = face2();
    if (face1() != test_face.transform(xform_3d(face1().centroid() - face2().centroid())))
      return false;

    type_     = shape_element_type::straight;
    axis_pos_ = face1().centroid();
    axis_dir_ = face2().centroid() - face1().centroid();
    return true;
  };

  bool geoinfo::ecal_info::shape_element::is_curved() {
    auto& f1 = face1();
    auto f2  = face2();
    auto h1  = f1.normal().Dot(f1.centroid());
    auto h2  = f2.normal().Dot(f2.centroid());
    auto dpr = f1.normal().Dot(f2.normal());
    auto det = (1 - dpr * dpr);

    if (is_zero_within_tolerance(det))
      return false;

    auto c1        = (h1 - h2 * dpr) / det;
    auto c2        = (h2 - h1 * dpr) / det;
    auto axis_pos_ = c1 * f1.normal() + c2 * f2.normal();
    auto axis_dir_ = f1.normal().Cross(f2.normal());
    auto angle     = TMath::ACos(f1.normal().Dot(f2.normal()));

    if (!is_zero_within_tolerance(std::fabs(angle) - 0.5 * TMath::Pi()))
      return false;

    if (f1 != f2.transform(xform_3d(rot_3d(rot_ang(axis_dir_, -angle)), axis_pos_)))
      return false;

    type_ = shape_element_type::curved;

    return true;
  };

  //////////////////////////////////////////////////////
  // geoinfo::ecal_info::module
  //////////////////////////////////////////////////////

  std::vector<geoinfo::ecal_info::cell> geoinfo::ecal_info::module::construct_cells() {
    order_elements();
    return std::vector<geoinfo::ecal_info::cell>();
  };

  void geoinfo::ecal_info::module::order_elements() {
    if (elements_.size() == 1)
      return;

    std::vector<shape_element> tmp;
    tmp.swap(elements_);

    auto insert = [&](const std::vector<shape_element>::iterator& it_insert,
                      const std::vector<shape_element>::iterator& it_erase) {
      elements_.insert(it_insert, *it_erase);
      tmp.erase(it_erase);
    };
    auto insert_front = [&](const std::vector<shape_element>::iterator& it) { insert(elements_.begin(), it); };
    auto insert_back  = [&](const std::vector<shape_element>::iterator& it) { insert(elements_.end(), it); };

    auto first_el       = tmp.begin();
    auto current_face_1 = first_el->face1();
    auto current_face_2 = first_el->face2();

    insert_front(first_el);

    while (tmp.size() != 0) {
      auto found = false;
      for (auto it = tmp.begin(); it != tmp.end(); it++) {
        if (current_face_1 == it->face2()) {
          found          = true;
          current_face_1 = it->face1();
          insert_front(it);
          break;
        } else if (current_face_1 == it->face1()) {
          found = true;
          it->swap_faces();
          current_face_1 = it->face1();
          insert_front(it);
          break;
        } else if (current_face_2 == it->face2()) {
          found = true;
          it->swap_faces();
          current_face_2 = it->face2();
          insert_back(it);
          break;
        } else if (current_face_2 == it->face1()) {
          found          = true;
          current_face_2 = it->face2();
          insert_back(it);
          break;
        }
      }

      if (found == false) {
        UFW_ERROR("Module disconnected: At least one shape element has not faces in common");
      }
    }
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
        m.add(el);
      } else if (shape->TestShapeBit(TGeoShape::kGeoBox)) {
        auto el = box_to_shape_element(static_cast<TGeoBBox*>(shape));
        el.transform(transf);
        m.add(el);
      } else {
        UFW_ERROR(fmt::format("Unexpected shape: {}", shape->GetName()));
      }
    });
    m.construct_cells();
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
    m.add(el);
    m.construct_cells();
  }

} // namespace sand
