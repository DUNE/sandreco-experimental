#pragma once

#include <common/truth.h>
#include <ufw/data.hpp>

namespace sand::grain {

  struct pictures :ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {

    static constexpr size_t COLS = 32;
    static constexpr size_t ROWS = 32;

    struct pixel : public sand::true_hits {
      double amplitude = 0.0;
      //do we want also time of first photon?
    };

    struct picture {
      uint16_t camera_id;
      std::string camera_name;
      double time_begin; // begin of slice
      double time_end; // end of slice
      std::array<pixel, COLS * ROWS> channels;
    };

    using picture_list = std::vector<picture>;

    picture_list pictures;

  };

}

UFW_DECLARE_MANAGED_DATA(sand::grain::pictures)
