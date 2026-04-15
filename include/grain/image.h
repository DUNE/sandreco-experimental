#pragma once

#include <common/truth.h>
#include <grain/grain.h>
#include <grain/photons.h>
#include <algorithm>
#include <memory>

namespace sand::grain {

  struct images : managed_data_base {
    using truth = sand::truth<hits::photon>;
    struct pixel : public truth {
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
      inline truth all_hits() const;
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

  inline images::truth images::image::all_hits() const {
    images::truth hits;
    for (const pixel& p : pixels) {
      hits.insert(p.true_hits());
    }
    return hits;
  }

} // namespace sand::grain

UFW_DECLARE_MANAGED_DATA(sand::grain::images)

// For dictionaries
UFW_DECLARE_UNMANAGED_DATA(sand::grain::images::pixel)
// FIXME this does not work because of commas. Find another class instead of SMatrix??
// UFW_DECLARE_UNMANAGED_DATA(ROOT::Math::SMatrix<sand::grain::images::pixel,32,32,ROOT::Math::MatRepStd<sand::grain::images::pixel,32,32>>)
UFW_DECLARE_UNMANAGED_DATA(sand::grain::pixel_array<sand::grain::images::pixel>)
