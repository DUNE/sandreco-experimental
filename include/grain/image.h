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

    using image_list = std::vector<std::vector<image>>; // Outer vector containing events in one spill

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

UFW_DECLARE_UNMANAGED_DATA(sand::grain::pixel_array<sand::grain::images::pixel>)

// FIXME Macro does not work because of commas. Typedef does not help, we need the correct stringified name.
// Macro expanded by hand. TODO keep in line with definition of macro
// UFW_DECLARE_UNMANAGED_DATA(ROOT::Math::SMatrix<sand::grain::images::pixel,32,32,
//                                                ROOT::Math::MatRepStd<sand::grain::images::pixel,32,32>>)
#ifdef UFW_IMPLEMENT_STREAMER_FOR_TYPE
static ufw::data::detail::rtti_helper<ROOT::Math::SMatrix<sand::grain::images::pixel,32,32,
                                      ROOT::Math::MatRepStd<sand::grain::images::pixel,32,32>>>
  UFW_PRIVATE_UNIQUE_NAME(ufw_rtti_helper_)("ROOT::Math::SMatrix<sand::grain::images::pixel,32,32,"
                                            "ROOT::Math::MatRepStd<sand::grain::images::pixel,32,32>>");
#endif // UFW_IMPLEMENT_STREAMER_FOR_TYPE
