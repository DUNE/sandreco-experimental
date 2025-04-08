#pragma once

#include <ufw/data.hpp>
#include <vector>
#include <cstdint>

namespace sand::grain {

  struct photons : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    struct pe
    {
      uint32_t h_index;
      double energy;
      double time;
      double x;
      double y;
      double z;
      double x_origin;
      double y_origin;
      double z_origin;
      double px;
      double py;
      double pz;
      double scatter;
      bool inside_camera;
    };

    struct camera
    {
      uint16_t camera_id;  // or std::string camera_name;
      std::vector<pe> photoelectrons;
    };
  };

  using optical_image = std::vector<camera>;
}

UFW_DECLARE_MANAGED_DATA(sand::grain::photons)
