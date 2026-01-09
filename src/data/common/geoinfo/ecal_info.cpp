#include <ecal_info.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>
#include <ufw/context.hpp>

namespace sand {

  geoinfo::ecal_info::cell_element_face::cell_element_face(const pos_3d& v1, const pos_3d& v2, const pos_3d& v3, const pos_3d& v4)
    : p1(v1), p2(v2), p3(v3), p4(v4) {
      // Check coplanarity: four points are coplanar if the scalar triple product is zero
      // (p2-p1) · ((p3-p1) × (p4-p1)) = 0
      dir_3d u = (p2 - p1).Unit();
      dir_3d w = (p3 - p1).Unit();
      dir_3d z = (p4 - p1).Unit();
      double triple_product = u.Dot(w.Cross(z));
      
      const double tolerance = 1e-10;

      UFW_ASSERT(std::abs(triple_product) < tolerance, fmt::format("cell_face: four points are not coplanar (triple product = {})", triple_product));

      normal_dir = normal();
    }
  
  dir_3d geoinfo::ecal_info::cell_element_face::normal() const {
      dir_3d u = p2 - p1;
      dir_3d w = p3 - p1;
      return u.Cross(w).Unit();
    };

  geoinfo::ecal_info::cell_element::cell_element(const cell_element_face& f1, const cell_element_face& f2)
    : face1(f1), face2(f2) {
      
      const double tolerance = 1e-10;
      if((face1.normal_dir - face2.normal_dir).R() < tolerance || 
         (face1.normal_dir + face2.normal_dir).R() < tolerance) {
        type = cell_element_type::straight; 
      }
      else if(face1.normal_dir.Cross(face2.normal_dir).R() - 1. < 0) {
        type = cell_element_type::curved;
      }
      else {
        UFW_EXCEPT(std::invalid_argument, "cell_element: faces normals are neither parallel nor orthogonal");
      }
    }

  geoinfo::ecal_info::ecal_info(const geoinfo& gi) : subdetector_info(gi, "kloe_calo_volume_PV_0") {
    UFW_INFO("ecal_info: constructed ECAL geoinfo");

    
    auto& tgm      = ufw::context::current()->instance<root_tgeomanager>();
    auto nav       = tgm.navigator();

    auto ecal_path = gi.root_path() / path();
    nav->cd(ecal_path);
    auto nd = nav->get_node();

    UFW_INFO(fmt::format("ECAL node name: {}", nd->GetName()));
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
