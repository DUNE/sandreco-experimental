#pragma once

#include <algorithm>
#include <ufw/data.hpp>
#include <common/truth.h>
#include <grain/grain.h>

namespace sand::grain {

  struct images : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {

    struct pixel : public sand::true_hits {
      double amplitude;
      double time_first;
    };

    struct image {
      uint16_t camera_id;
      std::string camera_name;
      double time_begin; // begin of slice
      double time_end; // end of slice
      pixel_array<pixel> pixels;
    public:
      inline pixel_array<double> amplitude_array();
      inline pixel_array<double> time_array();
      inline sand::true_hits all_hits();

    };

    using image_list = std::vector<image>;

    image_list pictures;

  };

  inline pixel_array<double> images::image::amplitude_array() {
    pixel_array<double> ret;
    std::transform(pixels.begin(), pixels.end(), ret.begin(), [](const pixel& p){ return p.amplitude; });
    return ret;
  }

  inline pixel_array<double> images::image::time_array() {
    pixel_array<double> ret;
    std::transform(pixels.begin(), pixels.end(), ret.begin(), [](const pixel& p){ return p.time_first; });
    return ret;
  }

  inline sand::true_hits images::image::all_hits() {
    sand::true_hits hits;
    for (const pixel& p : pixels) {
      hits.add(p.hits);
    }
    return hits;
  }

}

UFW_DECLARE_MANAGED_DATA(sand::grain::images)
