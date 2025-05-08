#include <TGeoManager.h>

#include <grain_manager.hpp>

namespace sand {

  geomanager::grain_manager::grain_manager(const geomanager& gm) : subdetector_manager(gm, "GRAIN_lv_PV_0/GRAIN_LAr_lv_PV_0") {}

}
