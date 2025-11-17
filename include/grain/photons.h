#pragma once

#include <vector>
#include <cstdint>

#include <ufw/data.hpp>
#include <common/truth.h>
#include <grain/grain.h>

namespace sand::grain {

  struct hits : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    struct photon : public true_hit
    {
      vec_4d pos;
      pos_3d origin;
      mom_4d p;
      double scatter;
      bool inside_camera;
      channel_id::link_t camera_id;
    };

    using photon_collection = std::vector<photon>;
    photon_collection photons;

  };

}

UFW_DECLARE_MANAGED_DATA(sand::grain::hits)
