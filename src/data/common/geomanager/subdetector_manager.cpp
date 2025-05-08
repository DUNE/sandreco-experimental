#include <TGeoManager.h>

#include <subdetector_manager.hpp>

namespace sand {

  geomanager::subdetector_manager::subdetector_manager(const geomanager& gm, const path& subpath) : r_mgr(gm) {
    path self = gm.root_path() / subpath;
    if (!gm.root_gm()->cd(self.c_str())) {
      UFW_EXCEPT(path_not_found, self);
    }
    double local[3] = { 0.0, 0.0, 0.0 };
    double master[3] = { 0.0, 0.0, 0.0 };
    gm.root_gm()->LocalToMaster(local, master);
    m_centre.SetCoordinates(master);

  }

}
