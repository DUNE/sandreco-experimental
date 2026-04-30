#include <grain/image.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

namespace sand::test {

  class test_grain_spill_slicer : public ufw::process {
   public:
    test_grain_spill_slicer();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
  };

  void test_grain_spill_slicer::configure(const ufw::config& cfg) {
    UFW_DEBUG("test_spill_slicer configured at: {}", fmt::ptr(this));
  }

  test_grain_spill_slicer::test_grain_spill_slicer() : process({{"images", "sand::grain::images"}}, {}) {
    UFW_INFO("Creating a test_spill_slicer process at {}", fmt::ptr(this));
  }

  void test_grain_spill_slicer::run() {
    UFW_DEBUG("test_spill_slicer run called with context_id: {}", ufw::context::current()->id());
    const auto& spill_images_in = get<sand::grain::images>("images").images;
    for (const auto& images_in : spill_images_in) {
      for (const auto& image : images_in) {
        // check image time in spill
        UFW_ASSERT(!std::isnan(image.time_begin) && !std::isnan(image.time_end), "Image begin time and/or end is NaN.");
        UFW_ASSERT(image.time_begin >= 0. && image.time_begin < 25000. && image.time_end > 0.
                       && image.time_end <= 25000.,
                   "Image not in spill duration. time_begin : {}, time_end :{}", image.time_begin, image.time_end);
        UFW_ASSERT(image.time_end > image.time_begin, "Image time_begin comes after time_end.");
        // check pixels
        for (const auto& pixel : image.pixels) {
          UFW_ASSERT(!std::isnan(pixel.amplitude) && pixel.amplitude >= 0, "Non-physical pixel amplitude: {}",
                     pixel.amplitude);
          UFW_ASSERT(pixel.amplitude == 0 && std::isnan(pixel.time_first),
                     "Time of first photon is not NaN, yet 0 photons were detected. time_first: {}", pixel.time_first);
          if (pixel.amplitude > 0) {
            UFW_ASSERT(!std::isnan(pixel.time_first) && pixel.time_first >= image.time_begin
                           && pixel.time_first <= image.time_end,
                       "Pixel time of first photon not in spill: {}", pixel.time_first);
          }
        }
      }
    }
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::test_grain_spill_slicer)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::test_grain_spill_slicer)
