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

    inline dir_3d r_wrt_axis(const pos_3d& p, const pos_3d& l_pos, const dir_3d& l_dir) {
      return (p - l_pos) - (p - l_pos).Dot(l_dir) / l_dir.R() * l_dir;
    }

    inline pos_3d z_wrt_axis(const pos_3d& p, const pos_3d& l_pos, const dir_3d& l_dir) {
      return p - r_wrt_axis(p, l_pos, l_dir);
    }

    double ang_wrt_axis(const pos_3d& p1, const pos_3d& p2, const pos_3d& l_pos, const dir_3d& l_dir) {
      auto r1 = r_wrt_axis(p1, l_pos, l_dir);
      auto r2 = r_wrt_axis(p2, l_pos, l_dir);
      return TMath::ACos(r1.Dot(r2) / (r1.R() * r2.R()));
    }

    void print_face(const geoinfo::ecal_info::shape_element_face& f) {
      UFW_DEBUG(fmt::format("Vertices:"));
      for (size_t i = 0; i < f.vtx().size(); i++)
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

    void print_module(const geoinfo::ecal_info::module& m) {
      std::string s;
      if (m.id().subdetector == geoinfo::ecal_info::subdetector_t::BARREL)
        s = "BARREL";
      else if (m.id().subdetector == geoinfo::ecal_info::subdetector_t::ENDCAP_A)
        s = "ENDCAP_A";
      else if (m.id().subdetector == geoinfo::ecal_info::subdetector_t::ENDCAP_B)
        s = "ENDCAP_B";
      UFW_DEBUG("Subdetector: {}; Module: {}", s, m.id().module);
      for (size_t i = 0; i < m.size(); i++) {
        UFW_DEBUG("Element: {}", i);
        print_element(m.at(i));
      }
    }

    void print_cell(const geoinfo::ecal_info::cell& c) {
      std::string s;
      if (c.id().module.subdetector == geoinfo::ecal_info::subdetector_t::BARREL)
        s = "BARREL";
      else if (c.id().module.subdetector == geoinfo::ecal_info::subdetector_t::ENDCAP_A)
        s = "ENDCAP_A";
      else if (c.id().module.subdetector == geoinfo::ecal_info::subdetector_t::ENDCAP_B)
        s = "ENDCAP_B";
      UFW_DEBUG("Subdetector: {}; Module: {}; Row: {}; Column: {}", s, c.id().module.module, c.id().row, c.id().column);
      for (size_t i = 0; i < c.size(); i++) {
        UFW_DEBUG("Element: {}", i);
        print_element(c.at(i));
      }
    }

  } // namespace

  //////////////////////////////////////////////////////
  // geoinfo::ecal_info::shape_element_face
  //////////////////////////////////////////////////////

  bool geoinfo::ecal_info::shape_element_face::are_points_coplanar() const {
    // Check coplanarity: four points are coplanar if the scalar triple product is zero
    // (p2-p1) · ((p3-p1) × (p4-p1)) = 0
    UFW_DEBUG("Checking if points are coplanar");
    dir_3d u              = (vtx(1) - vtx(0)).Unit();
    dir_3d w              = (vtx(2) - vtx(0)).Unit();
    dir_3d z              = (vtx(3) - vtx(0)).Unit();
    double triple_product = u.Dot(w.Cross(z));
    UFW_DEBUG("triple product: {}", triple_product);

    return is_zero_within_tolerance(triple_product);
  }

  geoinfo::ecal_info::shape_element_face::shape_element_face(const pos_3d& p1, const pos_3d& p2, const pos_3d& p3,
                                                             const pos_3d& p4)
    : v_{p1, p2, p3, p4} {
    UFW_ASSERT(are_points_coplanar(), std::string("cell_face: four points are not coplanar"));

    UFW_DEBUG("shape_element_face constructor");

    centroid_ = vtx(0) + 0.25 * ((vtx(1) - vtx(0)) + (vtx(2) - vtx(0)) + (vtx(3) - vtx(0)));

    dir_3d u = vtx(1) - vtx(0);
    dir_3d w = vtx(2) - vtx(0);
    normal_  = u.Cross(w).Unit();

    UFW_DEBUG("Centroid: {}", centroid_);
    UFW_DEBUG("Normal  : {}", normal_);
  }

  bool geoinfo::ecal_info::shape_element_face::operator== (const shape_element_face& other) const {
    UFW_DEBUG("face1:");
    // print_face(*this);
    UFW_DEBUG("face2:");
    // print_face(other);
    UFW_DEBUG("Checking faces are the same");

    auto mycopy_for = other;
    auto mycopy_rev = other;
    std::reverse(mycopy_rev.v_.begin(), mycopy_rev.v_.end());

    auto static same_elements = [](const shape_element_face& lhs, const shape_element_face& rhs) {
      for (auto const& pair : boost::combine(lhs.vtx(), rhs.vtx())) {
        pos_3d p1, p2;
        boost::tie(p1, p2) = pair;
        if (!is_zero_within_tolerance((p1 - p2).R()))
          return false;
      }
      return true;
    };

    for (int i = 0; i < vtx().size(); ++i) {
      if (same_elements(*this, mycopy_for) || same_elements(*this, mycopy_rev))
        return true;
      std::rotate(mycopy_for.v_.begin(), mycopy_for.v_.begin() + 1, mycopy_for.v_.end());
      std::rotate(mycopy_rev.v_.begin(), mycopy_rev.v_.begin() + 1, mycopy_rev.v_.end());
    }
    return false;
  }

  geoinfo::ecal_info::shape_element_face geoinfo::ecal_info::shape_element_face::transform(const xform_3d& transf) {
    UFW_DEBUG("Transform shape_element_face");
    UFW_DEBUG("Before: ");
    // print_face(*this);
    std::for_each(v_.begin(), v_.end(), [&](pos_3d& p) { p = transf * p; });
    centroid_ = transf * centroid();
    normal_   = transf * normal();
    UFW_DEBUG("After: ");
    // print_face(*this);
    return *this;
  }

  //////////////////////////////////////////////////////
  // geoinfo::ecal_info::shape_element
  //////////////////////////////////////////////////////

  geoinfo::ecal_info::shape_element::shape_element(const shape_element_face& f1, const shape_element_face& f2)
    : face1_(f1), face2_(f2) {
    UFW_DEBUG("shape_element constructor");
    if (!(is_straight() || is_curved())) {
      UFW_EXCEPT(std::invalid_argument, "cell_element: Neither a straight element nor a curved element!!");
    }
  }

  void geoinfo::ecal_info::shape_element::transform(const xform_3d& transf) {
    UFW_DEBUG("Transform shape_element");
    UFW_DEBUG("Before: ");
    // print_element(*this);
    face1_.transform(transf);
    face2_.transform(transf);
    axis_pos_ = transf * axis_pos();
    UFW_DEBUG("axis_dir_: {}", axis_dir_);
    UFW_DEBUG("transf   : {}", transf);
    axis_dir_ = transf * axis_dir();
    UFW_DEBUG("axis_dir_: {}", axis_dir_);
    UFW_ASSERT(axis_dir_.Mag2() > 0.1, fmt::format("Error axis dir is null!!! --> Dir: {}, {} --- {}", axis_dir_,
                                                   face1_.normal(), face2_.normal()));
    UFW_DEBUG("After: ");
    // print_element(*this);
  };

  const geoinfo::ecal_info::shape_element_face& geoinfo::ecal_info::shape_element::face(size_t face_id) const {
    switch (face_id) {
    case 1:
      return face1();
    case 2:
      return face2();
    default:
      UFW_ERROR(fmt::format("faceid can be either 1 or 2; provided value: {}", face_id));
    }
  };

  pos_3d geoinfo::ecal_info::shape_element::to_face(const pos_3d& p, size_t face_id) const {
    UFW_DEBUG("Propagate point {} to face {}", p, face_id);
    auto& f = face(face_id);
    if (type() == shape_element_type::straight) {
      ROOT::Math::Translation3D fullTransform(f.normal().Dot(f.centroid() - p) / f.normal().Dot(axis_dir()) * axis_dir());
      return fullTransform * p;

      // UFW_DEBUG("Propagate to {}", p + f.normal().Dot(f.centroid() - p) / f.normal().Dot(axis_dir()) * axis_dir());
      // return p + f.normal().Dot(f.centroid() - p) / f.normal().Dot(axis_dir()) * axis_dir();
    } else {
      UFW_DEBUG("Point    : {}", p);
      UFW_DEBUG("Centroid : {}", f.centroid());
      UFW_DEBUG("Axis Pos.: {}", axis_pos());
      UFW_DEBUG("Axis Dir.: {}", axis_dir());
      auto ang = ang_wrt_axis(p, f.centroid(), axis_pos(), axis_dir());
      UFW_DEBUG("Angle: {}", ang);
      auto trf = xform_3d(rot_3d(rot_ang(axis_dir(), -ang)), axis_pos() - orig);
      UFW_DEBUG("Matrice: {}", trf);
      UFW_DEBUG("Propagate to {}", trf * p);

      // 1. Create the rotation around the origin-aligned direction
      ROOT::Math::AxisAngle rotation(axis_dir(), -ang);

      // 2. Define translations
      ROOT::Math::Translation3D toOrigin(axis_pos(), orig);
      ROOT::Math::Translation3D fromOrigin(orig, axis_pos());

      // 3. Combine: TranslationBack * Rotation * TranslationToOrigin
      // Note: ROOT applies transformations from right to left in operator*
      ROOT::Math::Transform3D fullTransform = fromOrigin * rotation * toOrigin;
      UFW_DEBUG("Matrice: {}", fullTransform);
      UFW_DEBUG("Propagate to {}", fullTransform * p);

      return fullTransform * p;
    }
  };

  double geoinfo::ecal_info::shape_element::to_face(const pos_3d& p1, size_t face_id, pos_3d& p2) const {
    p2 = to_face(p1, face_id);
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
      UFW_DEBUG("r1: {}", r1);
      UFW_DEBUG("r2: {}", r2);
      UFW_DEBUG("z1: {}", z1);
      UFW_DEBUG("z2: {}", z2);
      if (!is_zero_within_tolerance(r1.R() - r2.R()) || !is_zero_within_tolerance((z1 - z2).R())) {
        UFW_ERROR(fmt::format("Points: {}, {} are not aligned along shape_element axis", p1, p2));
        return -1.;
      }
      auto ang = ang_wrt_axis(p1, p2, axis_pos(), axis_dir());
      return r1.R() * std::fabs(ang);
    }
  };

  bool geoinfo::ecal_info::shape_element::is_straight() {
    UFW_DEBUG("Checking if shape_element is straight");
    auto test_face = face2();

    if (face1() != test_face.transform(xform_3d(face1().centroid() - face2().centroid())))
      return false;

    UFW_DEBUG("shape_element is straight!!!");

    type_     = shape_element_type::straight;
    axis_pos_ = face1().centroid();
    axis_dir_ = face2().centroid() - face1().centroid();
    UFW_ASSERT(axis_dir_.Mag2() > 0.1, fmt::format("Error axis dir is null!!! --> Dir: {}, {} --- {}", axis_dir_,
                                                   face1().normal(), face1().normal()));
    return true;
  };

  bool geoinfo::ecal_info::shape_element::is_curved() {
    UFW_DEBUG("Checking if shape_element is curved");
    auto& f1 = face1();
    auto f2  = face2();
    auto h1  = f1.normal().Dot(f1.centroid());
    auto h2  = f2.normal().Dot(f2.centroid());
    auto dpr = f1.normal().Dot(f2.normal());
    auto det = (1 - dpr * dpr);

    if (is_zero_within_tolerance(det))
      return false;

    auto c1   = (h1 - h2 * dpr) / det;
    auto c2   = (h2 - h1 * dpr) / det;
    axis_pos_ = c1 * f1.normal() + c2 * f2.normal();
    axis_dir_ = f1.normal().Cross(f2.normal());
    UFW_ASSERT(axis_dir().Mag2() > 0.1,
               fmt::format("Error axis dir is null!!! --> Dir: {}, {} --- {}", axis_dir(), f1.normal(), f2.normal()));
    auto angle = TMath::ACos(f1.normal().Dot(f2.normal()));

    if (!is_zero_within_tolerance(std::fabs(angle) - 0.5 * TMath::Pi()))
      return false;

    // 1. Create the rotation around the origin-aligned direction
    ROOT::Math::AxisAngle rotation(axis_dir(), -angle);

    // 2. Define translations
    ROOT::Math::Translation3D toOrigin(axis_pos(), orig);
    ROOT::Math::Translation3D fromOrigin(orig, axis_pos());

    // 3. Combine: TranslationBack * Rotation * TranslationToOrigin
    // Note: ROOT applies transformations from right to left in operator*
    ROOT::Math::Transform3D fullTransform = fromOrigin * rotation * toOrigin;

    // if (f1 != f2.transform(xform_3d(rot_3d(rot_ang(axis_dir_, -angle)), axis_pos_ - orig)))
    if (f1 != f2.transform(fullTransform))
      return false;

    UFW_DEBUG("shape_element is curved!!!");

    type_ = shape_element_type::curved;

    return true;
  };

  //////////////////////////////////////////////////////
  // geoinfo::ecal_info::module::grid
  //////////////////////////////////////////////////////

  geoinfo::ecal_info::module::grid::grid(const pos_3d& p1, const pos_3d& p2, const pos_3d& p3, const pos_3d& p4,
                                         const std::vector<double>& div12, const std::vector<double>& div14)
  /*: nrow_(div12.size()),
  ncol_(div14.size()),
  nodes_((ncol_ + 1) * (nrow_ + 1)) */
  {
    UFW_DEBUG("Constructing grid");
    UFW_DEBUG("div12 size: {}", div12.size());
    UFW_DEBUG("div14 size: {}", div14.size());
    UFW_DEBUG("product: {}", (ncol_ + 1) * (nrow_ + 1));
    nrow_  = div12.size();
    ncol_  = div14.size();
    nodes_ = std::vector<pos_3d>((ncol_ + 1) * (nrow_ + 1));
    UFW_DEBUG("nrow_: {}", nrow_);
    UFW_DEBUG("ncol_: {}", ncol_);
    UFW_DEBUG("node_ size: {}", nodes_.size());
    UFW_DEBUG("Construct grid: {} x {}", nrow_, ncol_);
    UFW_DEBUG("Construct grid: {}, {}, {}, {}", p1, p2, p3, p4);
    auto cumulnorm = [](const std::vector<double>& v) {
      std::vector<double> r;
      r.push_back(0.);
      double sum   = std::accumulate(v.begin(), v.end(), 0.);
      double cumul = 0.;
      for (const auto& val : v) {
        r.push_back(cumul + val / sum);
        cumul += r.back();
      }
      return r;
    };

    auto scale12 = cumulnorm(div12);
    auto scale14 = cumulnorm(div14);

    auto create_node = [&](size_t i12, size_t i14) {
      auto pt1 = p1 + scale12[i12] * (p2 - p1);
      auto pt2 = p4 + scale12[i12] * (p3 - p4);
      return pt1 + scale14[i14] * (pt2 - pt1);
    };

    for (size_t i12 = 0; i12 <= nrow_; i12++)
      for (size_t i14 = 0; i14 <= ncol_; i14++) {
        UFW_DEBUG("Creating node: {}, {}", i12, i14);
        node(i12, i14) = create_node(i12, i14);
        UFW_DEBUG("Creating node: {}, {}: DONE", i12, i14);
      }
    UFW_DEBUG("Constructing grid: DONE");
  };

  geoinfo::ecal_info::shape_element_face geoinfo::ecal_info::module::grid::face(size_t irow, size_t icol) {
    return geoinfo::ecal_info::shape_element_face(node(irow, icol), node(irow + 1, icol), node(irow + 1, icol + 1),
                                                  node(irow, icol + 1));
  };

  //////////////////////////////////////////////////////
  // geoinfo::ecal_info::module
  //////////////////////////////////////////////////////

  void geoinfo::ecal_info::module::construct_cells(std::vector<geoinfo::ecal_info::cell>& cells) {
    UFW_DEBUG("Constructing cells");
    UFW_DEBUG("Order Elements");
    order_elements();
    UFW_DEBUG("Order Elements: DONE");
    UFW_DEBUG("Construct Grid");
    auto grid = construct_grid();
    UFW_DEBUG("Construct Grid: DONE");
    geoinfo::ecal_info::fiber* fib;

    UFW_DEBUG("Looping rows and columns");
    for (size_t irow = 0; irow < grid.nrow(); irow++)
      for (size_t icol = 0; icol < grid.ncol(); icol++) {
        UFW_DEBUG("Row: {}, Column: {}", irow, icol);
        switch (icol) {
        case 0:
        case 1:
          fib = &geoinfo::ecal_info::kfiber_plane12;
          break;
        case 2:
          fib = &geoinfo::ecal_info::kfiber_plane3;
          break;
        default:
          fib = &geoinfo::ecal_info::kfiber_plane45;
          break;
        }
        geoinfo::ecal_info::cell_id cid;
        cid.module = id();
        cid.row    = irow;
        cid.column = icol;
        auto f     = grid.face(irow, icol);
        cells.push_back(construct_cell(f, cid, *fib));
      }
    UFW_DEBUG("Looping rows and columns: DONE");
    UFW_DEBUG("Constructing cells: DONE");
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

  geoinfo::ecal_info::module::grid geoinfo::ecal_info::module::construct_grid() const {
    UFW_DEBUG("Constructing grid");
    static std::vector<double> layers{40., 40., 40., 40., 49.};
    auto& f = at(0).face1();
    if (id().subdetector == geoinfo::ecal_info::subdetector_t::BARREL) {
      UFW_DEBUG("Constructing grid for barrel module");
      int idx = 0;
      for (int i = 1; i < f.vtx().size(); i++) {
        if (f.side(i).R() < f.side(idx).R())
          idx = i;
      }
      UFW_DEBUG("Constructing grid for barrel module: almost done");
      return grid(f.vtx(idx++), f.vtx(idx++), f.vtx(idx++), f.vtx(idx++), std::vector<double>(12, 1.), layers);
    } else if (id().subdetector == geoinfo::ecal_info::subdetector_t::ENDCAP_A
               || id().subdetector == geoinfo::ecal_info::subdetector_t::ENDCAP_B) {
      UFW_DEBUG("Constructing grid for endcap module");
      auto length = [&](size_t i) {
        pos_3d p_start = f.vtx(i) + 0.5 * f.side(i);
        pos_3d p_end;
        double l = 0.;
        for (const auto& el : elements_) {
          l += el.to_face(p_start, 2, p_end);
          p_start = p_end;
        }
        return l;
      };
      size_t idx  = 0;
      double lmin = length(idx);
      for (int i = 1; i < f.vtx().size(); i++) {
        auto lel = length(i);
        if (lel < idx) {
          idx  = i;
          lmin = lel;
        }
      }
      int ncol = 0;
      switch (id().subdetector) {
      case 0:
      case 1:
      case 16:
      case 17:
        ncol = 6;
        break;
      case 12:
      case 13:
      case 14:
      case 15:
        ncol = 2;
        break;
      default:
        ncol = 3;
        break;
      }
      UFW_DEBUG("Constructing grid for endcap module: almost done");
      return grid(f.vtx(idx++), f.vtx(idx++), f.vtx(idx++), f.vtx(idx++), std::vector<double>(ncol, 1.), layers);
    }
  };

  geoinfo::ecal_info::cell geoinfo::ecal_info::module::construct_cell(const shape_element_face& f, cell_id id,
                                                                      const fiber& fib) const {
    UFW_DEBUG("Constructing cell: {}, {}, {}, {}", id.module.subdetector, id.module.module, id.row, id.column);
    geoinfo::ecal_info::cell c(id, fib);
    auto f1 = f;
    for (const auto& el : elements_) {
      auto f2 = el.to_face(f1, 2);
      c.add({f1, f2});
      std::swap(f1, f2);
    }
    UFW_DEBUG("Constructing cell: {}, {}, {}, {} --> DONE", id.module.subdetector, id.module.module, id.row, id.column);
    return c;
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
    UFW_DEBUG("find pattern");
    std::smatch m;
    if (regex_search(path, m, re_ecal_barrel_module)) {
      UFW_DEBUG("barrel module cells");
      barrel_module_cells(path);
      UFW_DEBUG("barrel module cells: DONE");
      return;
    } else if (regex_search(path, m, re_ecal_endcap_module)) {
      UFW_DEBUG("endcap module cells");
      endcap_module_cells(path);
      UFW_DEBUG("endcap module cells: DONE");
      return;
    } else {
      auto nav = ufw::context::current()->instance<root_tgeomanager>().navigator();
      nav->cd(path);
      nav->for_each_node([&](auto node) { find_pattern(path / std::string(node->GetName())); });
    }
    UFW_DEBUG("find pattern: DONE");
  }

  geoinfo::ecal_info::module_id geoinfo::ecal_info::to_module_id(const geo_path& path) {
    geoinfo::ecal_info::module_id mid;
    std::smatch m;
    if (regex_search(path, m, re_ecal_barrel_module)) {
      mid.module      = static_cast<geoinfo::ecal_info::module_t>(std::stoi(m[1]));
      mid.subdetector = geoinfo::ecal_info::subdetector_t::BARREL;
    } else if (regex_search(path, m, re_ecal_endcap_module)) {
      mid.subdetector = std::stoi(m[1]) == 0 ? geoinfo::ecal_info::subdetector_t::ENDCAP_A
                                             : geoinfo::ecal_info::subdetector_t::ENDCAP_B;
      mid.module      = static_cast<ecal_info::module_t>(std::stoi(m[2]) + std::stoi(m[3]) * 16);
    } else {
      UFW_ERROR("Path doesn't match any ECAL regex!!");
    }
    return mid;
  }

  void geoinfo::ecal_info::endcap_module_cells(const geo_path& path) {
    auto nav = ufw::context::current()->instance<root_tgeomanager>().navigator();
    nav->cd(path);
    module m(to_module_id(path));
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
    m.construct_cells(m_cells);
    // print_module(m);
  }

  void geoinfo::ecal_info::barrel_module_cells(const geo_path& path) {
    UFW_DEBUG("Constructing barrel module");
    module m(to_module_id(path));
    auto nav = ufw::context::current()->instance<root_tgeomanager>().navigator();
    nav->cd(path);
    auto shape      = nav->get_node()->GetVolume()->GetShape();
    auto geo_transf = nav->get_hmatrix();
    auto transf     = to_xform_3d(geo_transf);
    auto el         = trd2_to_shape_element(static_cast<TGeoTrd2*>(shape));
    el.transform(transf);
    m.add(el);
    UFW_DEBUG("Construct cells");
    m.construct_cells(m_cells);
    UFW_DEBUG("Construct cells: done");
    // print_module(m);
    UFW_DEBUG("Constructing barrel module: DONE");
  }

} // namespace sand
