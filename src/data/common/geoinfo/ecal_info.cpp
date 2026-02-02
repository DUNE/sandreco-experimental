#include <ecal_info.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>
#include <ufw/context.hpp>
#include <boost/range/combine.hpp>

#include <Math/AxisAngle.h>
#include <TGeoTrd2.h>
#include <TGeoTube.h>
#include <TMath.h>

namespace sand {

  using rot_ang = ROOT::Math::AxisAngle;
  using trl_3d  = ROOT::Math::Translation3D;

  using shape_element_face       = geoinfo::ecal_info::shape_element_face;
  using shape_element_base       = geoinfo::ecal_info::shape_element_base;
  using shape_element_straight   = geoinfo::ecal_info::shape_element_straight;
  using shape_element_curved     = geoinfo::ecal_info::shape_element_curved;
  using shape_element_collection = geoinfo::ecal_info::shape_element_collection;
  using grid                     = geoinfo::ecal_info::grid;
  using cell                     = geoinfo::ecal_info::cell;
  using module                   = geoinfo::ecal_info::module;
  using cell_id                  = geoinfo::ecal_info::cell_id;
  using module_id                = geoinfo::ecal_info::module_id;

  using p_shape_element_base = std::shared_ptr<shape_element_base>;
  using el_vec               = std::vector<p_shape_element_base>;
  using el_vec_it            = std::vector<p_shape_element_base>::iterator;

  //////////////////////////////////////////////////////
  // stuff
  //////////////////////////////////////////////////////

  // Equality operator
  inline bool operator== (cell_id lhs, cell_id rhs) { return lhs.raw == rhs.raw; }

  // Less-than operator for ordering
  inline bool operator< (cell_id lhs, cell_id rhs) { return lhs.raw < rhs.raw; }

  // Equality operator
  inline bool operator== (module_id lhs, module_id rhs) { return lhs.raw == rhs.raw; }

  // Less-than operator for ordering
  inline bool operator< (module_id lhs, module_id rhs) { return lhs.raw < rhs.raw; }

  namespace {
    constexpr double ktolerance(1e-10);
    constexpr pos_3d orig(0., 0., 0.);
    inline bool is_zero_within_tolerance(double value) { return std::abs(value) < ktolerance; };

    inline const shape_element_face& operator* (const xform_3d& transf, const shape_element_face& f) {
      return *(new shape_element_face(transf * f.vtx(0), transf * f.vtx(1), transf * f.vtx(2), transf * f.vtx(3)));
    }

    xform_3d to_xform_3d(const TGeoHMatrix& mat) {
      const Double_t* r = mat.GetRotationMatrix();
      const Double_t* t = mat.GetTranslation();

      rot_3d rotation(r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8]);
      dir_3d translation(t[0], t[1], t[2]);

      return xform_3d(rotation, translation);
    }

    bool is_straight(const shape_element_face& f1, const shape_element_face& f2) {
      return f1 == xform_3d(f1.centroid() - f2.centroid()) * f2;
    }

    bool is_curved(const shape_element_face& f1, const shape_element_face& f2) {
      auto h1  = f1.normal().Dot(f1.centroid());
      auto h2  = f2.normal().Dot(f2.centroid());
      auto dpr = f1.normal().Dot(f2.normal());
      auto det = (1 - dpr * dpr);

      if (is_zero_within_tolerance(det))
        return false;

      auto c1       = (h1 - h2 * dpr) / det;
      auto c2       = (h2 - h1 * dpr) / det;
      auto axis_pos = c1 * f1.normal() + c2 * f2.normal();
      auto axis_dir = f1.normal().Cross(f2.normal());
      auto angle    = TMath::ACos(f1.normal().Dot(f2.normal()));

      if (!is_zero_within_tolerance(std::fabs(angle) - 0.5 * TMath::Pi()))
        return false;

      rot_ang rotation(axis_dir, -angle);
      trl_3d toOrigin(-axis_pos);
      trl_3d fromOrigin(axis_pos);
      xform_3d fullTransform = fromOrigin * rotation * toOrigin;

      return f1 == fullTransform * f2;
    }

    p_shape_element_base construct_shape_element(const shape_element_face& f1, const shape_element_face& f2) {
      if (is_straight(f1, f2)) {
        return std::make_shared<shape_element_straight>(f1, f2);
      } else if (is_curved(f1, f2)) {
        return std::make_shared<shape_element_curved>(f1, f2);
      } else {
        UFW_EXCEPT(std::invalid_argument, "cell_element: Neither a straight element nor a curved element!!");
      }
    }

    p_shape_element_base tube_to_shape_element(TGeoTubeSeg* tube) {
      auto rmin = tube->GetRmin();
      auto rmax = tube->GetRmax();
      auto dz   = tube->GetDz();
      auto phi1 = tube->GetPhi1();
      auto phi2 = tube->GetPhi2();

      auto to_shape_element_face = [&](double phi) {
        return shape_element_face(
            pos_3d(rmin * std::cos(phi * TMath::DegToRad()), rmin * std::sin(phi * TMath::DegToRad()), dz),
            pos_3d(rmin * std::cos(phi * TMath::DegToRad()), rmin * std::sin(phi * TMath::DegToRad()), -dz),
            pos_3d(rmax * std::cos(phi * TMath::DegToRad()), rmax * std::sin(phi * TMath::DegToRad()), -dz),
            pos_3d(rmax * std::cos(phi * TMath::DegToRad()), rmax * std::sin(phi * TMath::DegToRad()), dz));
      };
      return construct_shape_element(to_shape_element_face(phi1), to_shape_element_face(phi2));
    }

    p_shape_element_base box_to_shape_element(TGeoBBox* box) {
      auto dx = box->GetDX();
      auto dy = box->GetDY();
      auto dz = box->GetDZ();

      return construct_shape_element(
          shape_element_face(pos_3d(-dx, -dy, dz), pos_3d(-dx, -dy, -dz), pos_3d(dx, -dy, -dz), pos_3d(dx, -dy, dz)),
          shape_element_face(pos_3d(-dx, dy, dz), pos_3d(-dx, dy, -dz), pos_3d(dx, dy, -dz), pos_3d(dx, dy, dz)));
    }

    p_shape_element_base trd2_to_shape_element(TGeoTrd2* trd) {
      auto dx1 = trd->GetDx1();
      auto dx2 = trd->GetDx2();
      auto dy1 = trd->GetDy1();
      auto dy2 = trd->GetDy2();
      auto dz  = trd->GetDz();

      return construct_shape_element(shape_element_face(pos_3d(-dx1, -dy1, -dz), pos_3d(dx1, -dy1, -dz),
                                                        pos_3d(dx2, -dy1, dz), pos_3d(-dx2, -dy1, dz)),
                                     shape_element_face(pos_3d(-dx1, dy1, -dz), pos_3d(dx1, dy1, -dz),
                                                        pos_3d(dx2, dy1, dz), pos_3d(-dx2, dy1, dz)));
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

    pos_3d project_to_plane(const pos_3d& p, const pos_3d& plane_pos, const dir_3d& plane_norm,
                            const dir_3d& proj_dir) {
      trl_3d fullTransform(plane_norm.Dot(plane_pos - p) / plane_norm.Dot(proj_dir) * proj_dir);
      return fullTransform * p;
    }

    bool segments_intersect(const pos_3d& p11, const pos_3d& p12, const pos_3d& p21, const pos_3d& p22) {
      auto s1  = p12 - p11;
      auto s2  = p22 - p21;
      auto x   = s1.Cross(s2);
      auto det = x.Mag2();
      if (is_zero_within_tolerance(det))
        return false;
      auto t = (p21 - p11).Cross(s2).Dot(x) / det;
      if (t >= 0. && t <= 1.)
        return true;
      else
        return false;
    }

  } // namespace

  //////////////////////////////////////////////////////
  // geoinfo::ecal_info::shape_element_face
  //////////////////////////////////////////////////////

  bool shape_element_face::are_points_coplanar() const {
    // Check coplanarity: four points are coplanar if the scalar triple product is zero
    // (p2-p1) · ((p3-p1) × (p4-p1)) = 0
    dir_3d u              = (vtx(1) - vtx(0)).Unit();
    dir_3d w              = (vtx(2) - vtx(0)).Unit();
    dir_3d z              = (vtx(3) - vtx(0)).Unit();
    double triple_product = u.Dot(w.Cross(z));
    return is_zero_within_tolerance(triple_product);
  }

  shape_element_face::shape_element_face(const pos_3d& p1, const pos_3d& p2, const pos_3d& p3, const pos_3d& p4)
    : v_{p1, p2, p3, p4} {
    UFW_ASSERT(are_points_coplanar(), std::string("cell_face: four points are not coplanar"));

    centroid_ = vtx(0) + 0.25 * ((vtx(1) - vtx(0)) + (vtx(2) - vtx(0)) + (vtx(3) - vtx(0)));

    dir_3d u = vtx(1) - vtx(0);
    dir_3d w = vtx(2) - vtx(0);
    normal_  = u.Cross(w).Unit();
  }

  bool shape_element_face::operator== (const shape_element_face& other) const {
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

  const shape_element_face& shape_element_face::transform(const xform_3d& transf) {
    std::for_each(v_.begin(), v_.end(), [&](pos_3d& p) { p = transf * p; });
    centroid_ = transf * centroid();
    normal_   = transf * normal();
    return *this;
  }

  //////////////////////////////////////////////////////
  // geoinfo::ecal_info::shape_element_base
  //////////////////////////////////////////////////////

  void shape_element_base::transform(const xform_3d& transf) {
    face1_.transform(transf);
    face2_.transform(transf);
    axis_pos_ = transf * axis_pos();
    axis_dir_ = transf * axis_dir();
  }

  double shape_element_base::total_pathlength() const { return pathlength(face1().centroid(), face2().centroid()); }

  bool shape_element_base::is_inside(const pos_3d& p) const {
    constexpr size_t face_id = 1;
    auto p_prj_face          = to_face(p, face_id);
    for (size_t i = 0; i < face(face_id).vtx().size(); i++) {
      if (segments_intersect(p_prj_face, face(face_id).centroid(), face(face_id).vtx(i), face(face_id).vtx(i + 1))) {
        return false;
      }
    }
    auto p_prj_axis = z_wrt_axis(p, axis_pos(), axis_dir());
    if ((p_prj_axis - face1().centroid()).R() + (p_prj_axis - face2().centroid()).R()
        > (face1().centroid() - face2().centroid()).R())
      return false;
    return true;
  }

  shape_element_face shape_element_base::to_face(const shape_element_face& f, size_t face_id) const {
    return shape_element_face(to_face(f.vtx(0), face_id), to_face(f.vtx(1), face_id), to_face(f.vtx(2), face_id),
                              to_face(f.vtx(3), face_id));
  }

  //////////////////////////////////////////////////////
  // geoinfo::ecal_info::shape_element_straight
  //////////////////////////////////////////////////////

  shape_element_straight::shape_element_straight(const shape_element_face& f1, const shape_element_face& f2)
    : shape_element_base(f1, f2) {
    type_     = shape_element_type::straight;
    axis_pos_ = face1().centroid();
    axis_dir_ = face2().centroid() - face1().centroid();
    axis_dir_ /= axis_dir_.R();
  }

  pos_3d shape_element_straight::to_face(const pos_3d& p, size_t face_id) const {
    auto& f = face(face_id);
    return project_to_plane(p, f.centroid(), f.normal(), axis_dir());
  }

  double shape_element_straight::pathlength(const pos_3d& p1, const pos_3d& p2) const {
    if (!is_zero_within_tolerance(axis_dir().Cross(p1 - p2).R())) {
      UFW_EXCEPT(std::invalid_argument, fmt::format("Points: {}, {} are not aligned along shape_element axis", p1, p2));
      return -1.;
    }
    return (p1 - p2).R();
  }

  pos_3d shape_element_straight::offset2position(double offset_from_center) const {
    auto center = face1().centroid() + 0.5 * (face2().centroid() - face1().centroid());
    return center + offset_from_center * (axis_dir() / axis_dir().R());
  }

  //////////////////////////////////////////////////////
  // geoinfo::ecal_info::shape_element_curved
  //////////////////////////////////////////////////////

  shape_element_curved::shape_element_curved(const shape_element_face& f1, const shape_element_face& f2)
    : shape_element_base(f1, f2) {
    auto h1   = face1().normal().Dot(face1().centroid());
    auto h2   = face2().normal().Dot(face2().centroid());
    auto dpr  = face1().normal().Dot(face2().normal());
    auto det  = (1 - dpr * dpr);
    auto c1   = (h1 - h2 * dpr) / det;
    auto c2   = (h2 - h1 * dpr) / det;
    axis_pos_ = c1 * face1().normal() + c2 * face2().normal();
    axis_dir_ = face1().normal().Cross(face2().normal());
    type_     = shape_element_type::curved;
  }

  pos_3d shape_element_curved::to_face(const pos_3d& p, size_t face_id) const {
    auto& f  = face(face_id);
    auto ang = ang_wrt_axis(p, f.centroid(), axis_pos(), axis_dir());
    rot_ang rotation(axis_dir(), ang);
    trl_3d toOrigin(axis_pos(), orig);
    trl_3d fromOrigin(orig, axis_pos());
    xform_3d fullTransform = fromOrigin * rotation * toOrigin;
    return fullTransform * p;
  }

  double shape_element_curved::pathlength(const pos_3d& p1, const pos_3d& p2) const {
    auto r1 = r_wrt_axis(p1, axis_pos(), axis_dir());
    auto r2 = r_wrt_axis(p2, axis_pos(), axis_dir());
    auto z1 = z_wrt_axis(p1, axis_pos(), axis_dir());
    auto z2 = z_wrt_axis(p2, axis_pos(), axis_dir());
    if (!is_zero_within_tolerance(r1.R() - r2.R()) || !is_zero_within_tolerance((z1 - z2).R())) {
      UFW_EXCEPT(std::invalid_argument, fmt::format("Points: {}, {} are not aligned along shape_element axis", p1, p2));
      return -1.;
    }
    auto ang = ang_wrt_axis(p1, p2, axis_pos(), axis_dir());
    return r1.R() * std::fabs(ang);
  }

  pos_3d shape_element_curved::offset2position(double offset_from_center) const {
    auto p1                     = face1().centroid();
    auto p2                     = face2().centroid();
    auto ang_center             = 0.5 * ang_wrt_axis(p1, p2, axis_pos(), axis_dir());
    auto ang_offset_from_center = offset_from_center / r_wrt_axis(p1, axis_pos(), axis_dir()).R();
    // the sign has to be checked!!
    rot_ang rotation(axis_dir(), ang_center + ang_offset_from_center);
    trl_3d toOrigin(axis_pos(), orig);
    trl_3d fromOrigin(orig, axis_pos());
    xform_3d fullTransform = fromOrigin * rotation * toOrigin;
    return fullTransform * p1;
  }

  //////////////////////////////////////////////////////
  // geoinfo::ecal_info::shape_element_collection
  //////////////////////////////////////////////////////

  void shape_element_collection::order_elements() {
    if (elements_.size() == 1)
      return;

    el_vec tmp;
    tmp.swap(elements_);

    auto insert = [&](const el_vec_it& it_insert, const el_vec_it& it_erase) {
      elements_.insert(it_insert, *it_erase);
      tmp.erase(it_erase);
    };
    auto insert_front = [&](const el_vec_it& it) { insert(elements_.begin(), it); };
    auto insert_back  = [&](const el_vec_it& it) { insert(elements_.end(), it); };

    auto first_el       = tmp.begin();
    auto current_face_1 = (*first_el)->face1();
    auto current_face_2 = (*first_el)->face2();

    insert_front(first_el);

    while (tmp.size() != 0) {
      auto found = false;
      for (auto it = tmp.begin(); it != tmp.end(); it++) {
        if (current_face_1 == (*it)->face2()) {
          found          = true;
          current_face_1 = (*it)->face1();
          insert_front(it);
          break;
        } else if (current_face_1 == (*it)->face1()) {
          found = true;
          (*it)->swap_faces();
          current_face_1 = (*it)->face1();
          insert_front(it);
          break;
        } else if (current_face_2 == (*it)->face2()) {
          found = true;
          (*it)->swap_faces();
          current_face_2 = (*it)->face2();
          insert_back(it);
          break;
        } else if (current_face_2 == (*it)->face1()) {
          found          = true;
          current_face_2 = (*it)->face2();
          insert_back(it);
          break;
        }
      }

      if (found == false) {
        UFW_EXCEPT(std::invalid_argument, "Module disconnected: At least one shape element has not faces in common");
      }
    }
  }

  double shape_element_collection::total_pathlength() const {
    double l = 0.;
    for (const auto& el : elements())
      l += el->total_pathlength();
    return l;
  }

  bool shape_element_collection::is_inside(const pos_3d& p) const {
    for (const auto& el : elements())
      if (el->is_inside(p))
        return true;
    return false;
  }

  //////////////////////////////////////////////////////
  // geoinfo::ecal_info::module::cell
  //////////////////////////////////////////////////////

  double cell::pathlength(const pos_3d& p, size_t face_id) const {
    double l  = 0.;
    pos_3d p1 = p;
    pos_3d p2;
    for (auto it = element_collection().elements().begin(); it != element_collection().elements().end(); it++) {
      if ((*it)->is_inside(p)) {
        if (face_id == 1) {
          for (auto itr = std::make_reverse_iterator(it + 1); itr != element_collection().elements().rend(); itr++) {
            p2 = (*itr)->to_face(p1, face_id);
            l += (*itr)->pathlength(p1, p2);
            std::swap(p1, p2);
          }
        } else if (face_id == 2) {
          for (auto itf = it; itf != element_collection().elements().end(); itf++) {
            p2 = (*itf)->to_face(p1, face_id);
            l += (*itf)->pathlength(p1, p2);
            std::swap(p1, p2);
          }
        } else {
          UFW_EXCEPT(std::invalid_argument, fmt::format("face_id can be either 1 or 2; Provided value: {}", face_id));
        }
      }
    }
    return l;
  }

  double cell::attenuation(double d) const {
    // dE/dx attenuation - Ea=p1*exp(-d/atl1)+(1.-p1)*exp(-d/atl2)
    //  d    distance from photocatode - 2 PMTs/cell; d1 and d2
    // atl1  50. cm
    // atl2  430 cm planes 1-2    innermost plane is 1
    //       380 cm plane 3
    //       330 cm planes 4-5
    //  p1   0.35
    auto& f = get_fiber();
    return f.fraction * TMath::Exp(-d / f.attenuation_length_1)
         + (1. - f.fraction) * TMath::Exp(-d / f.attenuation_length_2);
  }

  pos_3d cell::offset2position(double offset_from_center) const {
    if (offset_from_center < -0.5 * total_pathlength())
      return element_collection().elements().front()->face1().centroid();
    if (offset_from_center > 0.5 * total_pathlength())
      return element_collection().elements().back()->face2().centroid();
    auto from_face1 = 0.5 * total_pathlength() + offset_from_center;
    auto el_iter    = element_collection().elements().begin();

    while (from_face1 > (*el_iter)->total_pathlength() && el_iter != element_collection().elements().end()) {
      auto& el = *(*el_iter);
      from_face1 -= (*el_iter++)->total_pathlength();
    }
    return (*el_iter)->offset2position(from_face1 - 0.5 * (*el_iter)->total_pathlength());
  }

  //////////////////////////////////////////////////////
  // geoinfo::ecal_info::module::grid
  //////////////////////////////////////////////////////

  grid::grid(const pos_3d& p1, const pos_3d& p2, const pos_3d& p3, const pos_3d& p4, const std::vector<double>& div12,
             const std::vector<double>& div14) {
    ncol_          = div12.size();
    nrow_          = div14.size();
    nodes_         = std::vector<pos_3d>((ncol_ + 1) * (nrow_ + 1));
    auto cumulnorm = [](const std::vector<double>& v) {
      std::vector<double> r{0.};
      double sum = std::accumulate(v.begin(), v.end(), 0.);
      for (const auto& val : v)
        r.push_back(r.back() + val / sum);
      return r;
    };

    auto scale12 = cumulnorm(div12);
    auto scale14 = cumulnorm(div14);

    auto create_node = [&](size_t i12, size_t i14) {
      auto pt1       = p1 + scale12[i12] * (p2 - p1);
      auto pt2       = p4 + scale12[i12] * (p3 - p4);
      node(i12, i14) = pt1 + scale14[i14] * (pt2 - pt1);
    };

    for (size_t i12 = 0; i12 <= ncol_; i12++)
      for (size_t i14 = 0; i14 <= nrow_; i14++)
        create_node(i12, i14);
  }

  shape_element_face grid::face(size_t icol, size_t irow) const {
    return shape_element_face(get_node(icol, irow), get_node(icol + 1, irow), get_node(icol + 1, irow + 1),
                              get_node(icol, irow + 1));
  }

  //////////////////////////////////////////////////////
  // geoinfo::ecal_info::module
  //////////////////////////////////////////////////////

  grid module::construct_grid(const std::vector<double> col_widths, const std::vector<double> row_widths) const {
    auto& f    = element_collection().at(0).face1();
    size_t idx = 0;
    if (id().subdetector == geoinfo::ecal_info::subdetector_t::BARREL) {
      idx = 0;
      if (is_zero_within_tolerance(f.side(0).R() - f.side(2).R())) {
        if (f.side(1).R() < f.side(3).R())
          idx = 1;
        else
          idx = 3;
      } else if (is_zero_within_tolerance(f.side(1).R() - f.side(3).R())) {
        if (f.side(0).R() < f.side(2).R())
          idx = 0;
        else
          idx = 2;
      } else {
        UFW_EXCEPT(std::invalid_argument, fmt::format("Unexpected shape for barrel module!!"));
      }
    } else if (id().subdetector == geoinfo::ecal_info::subdetector_t::ENDCAP_A
               || id().subdetector == geoinfo::ecal_info::subdetector_t::ENDCAP_B) {
      auto length = [&](size_t i) {
        pos_3d p_start = f.vtx(i) + 0.5 * f.side(i);
        pos_3d p_end;
        double l = 0.;
        for (const auto& el : element_collection().elements()) {
          auto p_end = el->to_face(p_start, 2);
          l += el->pathlength(p_start, p_end);
          p_start = p_end;
        }
        return l;
      };

      idx         = 0;
      double lmax = length(idx);

      for (int i = 1; i < f.vtx().size(); i++) {
        auto lel = length(i);
        if (lel > lmax) {
          idx  = i;
          lmax = lel;
        }
      }
    }
    return grid(f.vtx(idx), f.vtx(idx + 1), f.vtx(idx + 2), f.vtx(idx + 3), col_widths, row_widths);
  }

  cell module::construct_cell(const shape_element_face& f, cell_id id, const fiber& fib) const {
    cell c(id, fib);
    auto f1 = f;
    for (const auto& el : element_collection().elements()) {
      auto f2 = el->to_face(f1, 2);
      if (el->type() == geoinfo::ecal_info::shape_element_type::straight)
        c.add(std::make_shared<shape_element_straight>(f1, f2));
      else
        c.add(std::make_shared<shape_element_curved>(f1, f2));
      std::swap(f1, f2);
    }
    return c;
  }

  //////////////////////////////////////////////////////
  // geoinfo::ecal_info
  //////////////////////////////////////////////////////

  geoinfo::ecal_info::ecal_info(const geoinfo& gi) : subdetector_info(gi, "kloe_calo_volume_PV_0") {
    find_modules(gi.root_path() / path());
  }

  geoinfo::ecal_info::~ecal_info() = default;

  const cell& geoinfo::ecal_info::at(const pos_3d& p) const {
    auto nav = ufw::context::current()->instance<root_tgeomanager>().navigator();
    nav->FindNode(p.X(), p.Y(), p.Z());
    auto path = nav->GetPath();
    auto gid  = id(geo_path(path));
    for (auto c : m_cells_map.at(gid))
      if (c->second.is_inside(p))
        return c->second;
    UFW_EXCEPT(std::invalid_argument,
               fmt::format("Point: {} in path: {} is not in any cell related to geo_id: {}", p, path, gid));
  }

  const cell& geoinfo::ecal_info::get_cell(cell_id cid) const {
    module_id mid;
    mid.module      = geoinfo::ecal_info::module_t(cid.module);
    mid.subdetector = geoinfo::ecal_info::subdetector_t(cid.subdetector);
    if (!m_modules_cells_maps.count(mid)) {
      UFW_EXCEPT(std::invalid_argument, fmt::format("Module: {} not found in the map: m_modules_cells_maps", mid.raw));
    }
    if (!m_modules_cells_maps.at(mid).count(cid)) {
      UFW_EXCEPT(std::invalid_argument,
                 fmt::format("Cell: {} not found in the map: m_modules_cells_maps.at({})  -> Cell: {}, Sub: {}, Mod: "
                             "{}, Cel: {}, Row: {}",
                             cid.raw, mid.raw, cid.raw, cid.subdetector, cid.module, cid.column, cid.row));
    }
    return m_modules_cells_maps.at(mid).at(cid);
  }

  geo_id geoinfo::ecal_info::id(const geo_path& path) const {
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
        gi.ecal.plane   = static_cast<sand::geo_id::plane_t>(std::stoi(m[13]));
      } else if (m[4] == "curv" && std::stoi(m[6]) == 0) {
        gi.ecal.plane   = static_cast<sand::geo_id::plane_t>(std::stoi(m[11]));
        gi.ecal.element = sand::geo_id::element_t::ENDCAP_CURVE_TOP;
      } else if (m[4] == "curv" && std::stoi(m[6]) == 1) {
        gi.ecal.plane   = static_cast<sand::geo_id::plane_t>(std::stoi(m[11]));
        gi.ecal.element = sand::geo_id::element_t::ENDCAP_CURVE_BOT;
      } else if (m[4] == "hor" && std::stoi(m[6]) == 0) {
        gi.ecal.plane   = static_cast<sand::geo_id::plane_t>(std::stoi(m[13]));
        gi.ecal.element = sand::geo_id::element_t::ENDCAP_HOR_TOP;
      } else if (m[4] == "hor" && std::stoi(m[6]) == 1) {
        gi.ecal.plane   = static_cast<sand::geo_id::plane_t>(std::stoi(m[13]));
        gi.ecal.element = sand::geo_id::element_t::ENDCAP_HOR_BOT;
      } else {
        UFW_EXCEPT(std::invalid_argument, fmt::format("Unknown ECAL endcap element type: {}", m[4].str()));
      }
    } else {
      UFW_EXCEPT(std::invalid_argument,
                 fmt::format("Provided geo_path {} does not match ECAL sensible volume pattern", path));
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
        UFW_EXCEPT(std::invalid_argument, "Invalid ECAL endcap element type");
      }
      gp /= fmt::format(
          "ECAL_endcap_lv_PV_{}/ECAL_ec_mod_{}_lv_PV_{}/ECAL_ec_mod_{}_{}_lv_PV_{}/endvolECAL{}ActiveSlab_{}_PV_{}",
          index1, id.ecal.supermodule % 16, id.ecal.supermodule / 16, str1, id.ecal.supermodule, index2, str2,
          id.ecal.supermodule, str3, index3);
    } else {
      UFW_EXCEPT(std::invalid_argument, "Invalid ECAL region type");
    }
    return gp;
  }

  void geoinfo::ecal_info::find_modules(const geo_path& path) {
    std::smatch m;
    if (regex_search(path, m, re_ecal_barrel_module)) {
      barrel_module_cells(path);
      find_active_volumes(path, re_ecal_barrel_sensible_volume);
      return;
    } else if (regex_search(path, m, re_ecal_endcap_module)) {
      endcap_module_cells(path);
      find_active_volumes(path, re_ecal_endcap_sensible_volume);
      return;
    } else {
      auto nav = ufw::context::current()->instance<root_tgeomanager>().navigator();
      nav->cd(path);
      nav->for_each_node([&](auto node) { find_modules(path / std::string(node->GetName())); });
    }
  }

  void geoinfo::ecal_info::find_active_volumes(const geo_path& path, const std::regex& re) {
    std::smatch m;
    if (regex_search(path, m, re)) {
      auto gid = id(path);
      module_id mid;
      mid.subdetector = geoinfo::ecal_info::subdetector_t(gid.ecal.region);
      mid.module      = geoinfo::ecal_info::module_t(gid.ecal.supermodule);
      cell_id cid;
      cid.subdetector = geoinfo::ecal_info::subdetector_t(gid.ecal.region);
      cid.module      = geoinfo::ecal_info::module_t(gid.ecal.supermodule);
      cid.row         = uint8_t(gid.ecal.plane / uint8_t(40));
      cid.row         = (cid.row == 5) ? 4 : cid.row;
      cid.column      = 0;
      while (m_modules_cells_maps.at(mid).count(cid)) {
        m_cells_map[gid].push_back(m_modules_cells_maps.at(mid).find(cid));
        cid.column += 1;
      }
      return;
    } else {
      auto nav = ufw::context::current()->instance<root_tgeomanager>().navigator();
      nav->cd(path);
      nav->for_each_node([&](auto node) { find_active_volumes(path / std::string(node->GetName()), re); });
    }
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
      UFW_EXCEPT(std::invalid_argument, "Path doesn't match any ECAL regex!!");
    }
    return mid;
  }

  void geoinfo::ecal_info::construct_module_cells(const module& m, const grid& g) {
    geoinfo::ecal_info::fiber* fib;

    for (size_t irow = 0; irow < g.nrow(); irow++)
      for (size_t icol = 0; icol < g.ncol(); icol++) {
        switch (irow) {
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
        cid.subdetector = m.id().subdetector;
        cid.module      = m.id().module;
        cid.row         = irow;
        cid.column      = icol;
        auto f          = g.face(icol, irow);
        m_modules_cells_maps[m.id()].emplace(cid, m.construct_cell(f, cid, *fib));
      }
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
        el->transform(transf);
        m.add(el);
      } else if (shape->TestShapeBit(TGeoShape::kGeoBox)) {
        auto el = box_to_shape_element(static_cast<TGeoBBox*>(shape));
        el->transform(transf);
        m.add(el);
      } else {
        UFW_EXCEPT(std::invalid_argument, fmt::format("Unexpected shape: {}", shape->GetName()));
      }
    });

    m.order_elements();
    int ncol = 0;
    switch (m.id().module) {
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
    case 28:
    case 29:
    case 30:
    case 31:
      ncol = 2;
      break;
    default:
      ncol = 3;
      break;
    }
    static std::vector<double> row_widths{40., 40., 40., 40., 49.};
    std::vector<double> col_widths(ncol, 1.);
    auto grid = m.construct_grid(col_widths, row_widths);
    construct_module_cells(m, grid);
  }

  void geoinfo::ecal_info::barrel_module_cells(const geo_path& path) {
    module m(to_module_id(path));
    auto nav = ufw::context::current()->instance<root_tgeomanager>().navigator();
    nav->cd(path);
    auto shape      = nav->get_node()->GetVolume()->GetShape();
    auto geo_transf = nav->get_hmatrix();
    auto transf     = to_xform_3d(geo_transf);
    auto el         = trd2_to_shape_element(static_cast<TGeoTrd2*>(shape));
    el->transform(transf);
    m.add(el);
    m.order_elements();
    static std::vector<double> row_widths{40., 40., 40., 40., 49.};
    static std::vector<double> col_widths(12, 1.);
    auto grid = m.construct_grid(col_widths, row_widths);
    construct_module_cells(m, grid);
  }
} // namespace sand
