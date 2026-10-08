#pragma once

#include <common/data.h>
#include <grain/grain.h>

namespace sand::grain {

  struct photon : public sand::truth_index {
    vec_4d pos;
    pos_3d origin;
    mom_4d p;
    double scatter;
    bool inside_camera;
    channel_id::link_t camera_id;
  };

} // namespace sand::grain

SAND_DATA_COLLECTION(sand::grain, photon, photons)
