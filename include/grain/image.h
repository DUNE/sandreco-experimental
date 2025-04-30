#pragma once

#include <algorithm>
#include <memory>
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
      inline void blank(); //call blank if you are not already assigning every pixel
      template <typename T>
      pixel_array<T> amplitude_array() const;
      template <typename T>
      pixel_array<T> time_array() const;
      inline sand::true_hits all_hits() const;

    };

    using image_list = std::vector<image>;

    image_list images;

  };

  inline void images::image::blank() {
    std::uninitialized_fill(pixels.begin(), pixels.end(), pixel{{}, 0., NAN});
  }

  template <typename T>
  pixel_array<T> images::image::amplitude_array() const {
    pixel_array<T> ret;
    std::transform(pixels.begin(), pixels.end(), ret.begin(), [](const pixel& p){ return p.amplitude; });
    return ret;
  }

  template <typename T>
  pixel_array<T> images::image::time_array() const {
    pixel_array<T> ret;
    std::transform(pixels.begin(), pixels.end(), ret.begin(), [](const pixel& p){ return p.time_first; });
    return ret;
  }

  inline sand::true_hits images::image::all_hits() const {
    sand::true_hits hits;
    for (const pixel& p : pixels) {
      hits.add(p.hits);
    }
    return hits;
  }

}

UFW_DECLARE_MANAGED_DATA(sand::grain::images)
