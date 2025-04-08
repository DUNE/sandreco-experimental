#pragma once

#include <ufw/data.hpp>
#include <vector>
#include <cstdint>

namespace sand::grain {

  struct digi : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    struct channel
    {
       uint16_t channel_id;
       double time_rising_edge;
       double time_over_threshold;
       double charge;
       std::vector<uint32_t> h_indices; 
    };

    struct camera
    {
      uint16_t camera_id;  // or std::string camera_name;
      std::vector<channel> channels;
    };
  };

  using digitized_image = std::vector<digi::camera>;

}

UFW_DECLARE_MANAGED_DATA(sand::grain::digi)
