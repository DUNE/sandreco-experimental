#pragma once

#include <common/timerange.h>
#include <grain/digi.h>
#include <grain/grain.h>
#include <grain/photons.h>

#include <algorithm>
#include <memory>

namespace sand::grain {

  struct pixel : public sand::truth<photon> {
    double amplitude;
    double time_first;
  };

  struct image {
    channel_id::link_t camera_id;
    reco::timerange range;
    pixel_array<pixel> pixels;

   public:
    double t() const { return range.best(); }
    inline void blank(); // call blank if you are not already assigning every pixel
    template <typename T>
    pixel_array<T> amplitude_array() const;
    template <typename T>
    pixel_array<T> time_array() const;
    inline sand::truth<photon> all_hits() const;
  };

  inline void image::blank() { std::uninitialized_fill(pixels.begin(), pixels.end(), pixel{{}, 0., NAN}); }

  template <typename T>
  pixel_array<T> image::amplitude_array() const {
    pixel_array<T> ret;
    std::transform(pixels.begin(), pixels.end(), ret.begin(), [](const pixel& p) { return p.amplitude; });
    return ret;
  }

  template <typename T>
  pixel_array<T> image::time_array() const {
    pixel_array<T> ret;
    std::transform(pixels.begin(), pixels.end(), ret.begin(), [](const pixel& p) { return p.time_first; });
    return ret;
  }

  inline sand::truth<photon> image::all_hits() const {
    sand::truth<photon> true_hits;
    for (const pixel& p : pixels) {
      true_hits.insert(p.true_hits());
    }
    return true_hits;
  }

} // namespace sand::grain

SAND_DATA_COLLECTION(sand::grain, image, images)

// For dictionaries
UFW_DECLARE_UNMANAGED_DATA(sand::truth<sand::grain::photon>)
UFW_DECLARE_UNMANAGED_DATA(sand::grain::pixel)
UFW_DECLARE_UNMANAGED_DATA(sand::grain::pixel_array<sand::grain::pixel>)
