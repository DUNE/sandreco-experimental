#pragma once

#include <ufw/data.hpp>
#include <common/truth.h>
#include <vector>
#include <cstdint>

namespace sand::grain {

  struct digi : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    struct channel : public sand::true_hits
    {
       uint16_t channel_id;
       double time_rising_edge;
       double time_over_threshold;
       double charge;
    };

    struct image
    {
      uint16_t camera_id;  // or std::string camera_name;
      std::vector<channel> channels;
    };

    using image_list = std::vector<digi::image>;

    image_list images;

  };

}

UFW_DECLARE_MANAGED_DATA(sand::grain::digi)
