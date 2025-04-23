#pragma once

#include <ufw/data.hpp>
#include <common/truth.h>
#include <vector>
#include <cstdint>

namespace sand::grain {

  struct digi : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    struct photoelectron : public sand::true_hits
    {
       uint16_t channel_id;
       double time_rising_edge;
       double time_over_threshold;
       double charge;
    };

    struct camera
    {
      uint16_t camera_id;  
      std::string camera_name;
      std::vector<photoelectron> photoelectrons;
    };

    using camera_list = std::vector<camera>;

    camera_list cameras;

  };

}

UFW_DECLARE_MANAGED_DATA(sand::grain::digi)
