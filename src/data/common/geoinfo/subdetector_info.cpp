#include <root_tgeomanager/root_tgeomanager.hpp>
#include <subdetector_info.hpp>
#include <ufw/context.hpp>

#include <regex>

namespace sand {

  geoinfo::subdetector_info::subdetector_info(const geoinfo& gi, const geo_path& subpath)
    : r_info(gi), m_path(subpath) {
    auto& tgm = ufw::context::current()->instance<root_tgeomanager>();
    auto nav  = tgm.navigator();
    try {
      nav->cd(r_info.root_path() / m_path);
    } catch (const ufw::exception&) {
      UFW_EXCEPT(path_not_found, r_info.root_path() / m_path);
    }
    UFW_DEBUG("Using subdetector path '{}'.", m_path.c_str());
    TGeoHMatrix hm = nav->get_hmatrix();
    // I cry everytime... It's the same library...
    const double* rot  = hm.GetRotationMatrix();
    const double* tran = hm.GetTranslation();
    m_transform.SetComponents(rot[0], rot[1], rot[2], tran[0], rot[3], rot[4], rot[5], tran[1], rot[6], rot[7], rot[8],
                              tran[2]);
    // At least this part is decent, leave it as cross-check
    pos_3d c{0, 0, 0};
    dir_3d x{1, 0, 0};
    dir_3d y{0, 1, 0};
    dir_3d z{0, 0, 1};
    c = m_transform * c;
    x = m_transform * x;
    y = m_transform * y;
    z = m_transform * z;
    UFW_DEBUG("Initializing {} with local coordinates mapped as follows: ", m_path);
    UFW_DEBUG("c = ({:.2f}, {:.2f}, {:.2f})", c.x(), c.y(), c.z());
    UFW_DEBUG("x = ({:.2f}, {:.2f}, {:.2f})", x.x(), x.y(), x.z());
    UFW_DEBUG("y = ({:.2f}, {:.2f}, {:.2f})", y.x(), y.y(), y.z());
    UFW_DEBUG("z = ({:.2f}, {:.2f}, {:.2f})", z.x(), z.y(), z.z());
  }

  geoinfo::subdetector_info::~subdetector_info() = default;

} // namespace sand
