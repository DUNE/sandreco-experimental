#pragma once

#include <ufw/data.hpp>
#include <common/truth.h>
#include <grain/grain.h>
#include <algorithm>
#include <memory>

namespace sand::grain {

  struct images : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    struct pixel : public sand::truth {
      double amplitude;
      double time_first;
    };

    struct image {
      channel_id::link_t camera_id;
      double time_begin; // begin of slice
      double time_end;   // end of slice
      pixel_array<pixel> pixels;

     public:
      inline void blank(); // call blank if you are not already assigning every pixel
      template <typename T>
      pixel_array<T> amplitude_array() const;
      template <typename T>
      pixel_array<T> time_array() const;
      inline sand::truth all_hits() const;
    };

    using image_list = std::vector<image>;

    image_list images;
  };

  inline void images::image::blank() { std::uninitialized_fill(pixels.begin(), pixels.end(), pixel{{}, 0., NAN}); }

  template <typename T>
  pixel_array<T> images::image::amplitude_array() const {
    pixel_array<T> ret;
    std::transform(pixels.begin(), pixels.end(), ret.begin(), [](const pixel& p) { return p.amplitude; });
    return ret;
  }

  template <typename T>
  pixel_array<T> images::image::time_array() const {
    pixel_array<T> ret;
    std::transform(pixels.begin(), pixels.end(), ret.begin(), [](const pixel& p) { return p.time_first; });
    return ret;
  }

  inline sand::truth images::image::all_hits() const {
    sand::truth hits;
    for (const pixel& p : pixels) {
      hits.insert(p.true_hits());
    }
    return hits;
  }

} // namespace sand::grain

UFW_DECLARE_MANAGED_DATA(sand::grain::images)
