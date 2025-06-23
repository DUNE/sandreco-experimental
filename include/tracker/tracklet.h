#pragma once

#include <ufw/data.hpp>

#include <common/sand.h>
#include <common/truth.h>

namespace sand::tracker {

  struct tracklets : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {

    struct tracklet : public true_hits {
      pos_3d offset; ///< position of centre of tracklet, in global coordinates
      double sigma_pos; ///< position error of centre of tracklet, 1-sigma radius around offset
      dir_3d segment; ///< normalized direction of tracklet, in global coordinates
      double sigma_dir; ///< direction error, 1-sigma half-angle of cone around segment
      double quality; ///< fit quality, Chi^2/NDF, expected <= ~1 for a good tracklet
    };

    using tracklet_list = std::vector<tracklet>;

    tracklet_list tracklets;

  };

}

UFW_DECLARE_MANAGED_DATA(sand::tracker::tracklets)
