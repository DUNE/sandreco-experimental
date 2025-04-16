#pragma once

#include <ufw/data.hpp>
#include <common/sand.h>
#include <common/truth.h>
#include <vector>
#include <cstdint>

namespace sand::grain {

  struct hits : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    struct photon : public true_hit
    {
      pos_4d pos;
      pos_3d origin;
      mom_4d p;
      double scatter;
      bool inside_camera;
    };

    struct camera
    {
      uint16_t camera_id;  // or std::string camera_name;
      std::string camera_name;
      std::vector<photon> photons;
    };

    using camera_list = std::vector<camera>;

    camera_list cameras;

  };

}

UFW_DECLARE_MANAGED_DATA(sand::grain::hits)
