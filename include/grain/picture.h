#pragma once

#include <common/truth.h>
#include <ufw/data.hpp>

namespace sand::grain {

  struct pictures : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {

    static constexpr size_t heigth = 32;
    static constexpr size_t width = 32;

    struct pixel : public sand::true_hits {
      double amplitude;
    };

    struct picture {
      uint16_t camera_id;  // or std::string camera_name;
      double first_time; //time of the first photon
      double last_time; //time after the last photon
      std::array<pixel, heigth * width> channels;
    };

    using picture_list = std::vector<picture>;

    picture_list pictures;

  };

}
