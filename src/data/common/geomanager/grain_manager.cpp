#include <TGeoManager.h>

#include <grain_manager.hpp>

namespace sand {

  geomanager::grain_manager::grain_manager(const geomanager& gm, TGeoManager* tgeo) : r_mgr(gm) {
    path self = gm.root_path() / "GRAIN_lv_PV_0/GRAIN_LAr_lv_PV_0";
    tgeo->cd(self.c_str());
    double local[3] = { 0.0, 0.0, 0.0 };
    double master[3] = { 0.0, 0.0, 0.0 };
    tgeo->LocalToMaster(local, master);
    m_centre.SetCoordinates(master);

  }

}
