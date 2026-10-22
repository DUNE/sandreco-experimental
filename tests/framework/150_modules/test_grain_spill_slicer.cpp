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

  test_grain_spill_slicer::test_grain_spill_slicer() : process({{"images", "sand::grain::images"},
                                                                {"timeranges", "sand::reco::timeranges"}}, {}) {
    UFW_INFO("Creating a test_spill_slicer process at {}", fmt::ptr(this));
  }

  void test_grain_spill_slicer::run() {
    UFW_DEBUG("test_spill_slicer run called with context_id: {}", ufw::context::current()->id());
    const auto& spill_images_in = get<sand::grain::images>("images");
    const auto& trs_in = get<sand::reco::timeranges>("timeranges");
    for (const auto& image : spill_images_in) {
      // check image time in spill
      UFW_ASSERT(image.range.earliest() >= 0. && image.range.earliest() < 25000. && image.range.latest() > 0.
                     && image.range.latest() <= 25000.,
                 "Image time {} not in spill duration", image.range);
      // check pixels
      for (const auto& pixel : image.pixels) {
        UFW_ASSERT(pixel.amplitude >= 0.0, "Non-physical pixel amplitude: {}", pixel.amplitude);
        UFW_ASSERT(pixel.amplitude > 0.0 || std::isnan(pixel.time_first),
                   "Time of first photon is not NaN, yet 0 photons were detected. time_first: {}", pixel.time_first);
        if (pixel.amplitude > 0.0) {
          UFW_ASSERT(!std::isnan(pixel.time_first),
                     "Time of first photon is NaN, yet {} photons were detected. time_first: {}", pixel.amplitude);
          UFW_ASSERT(image.range.contains(pixel.time_first),
                     "Pixel time of first photon not in image declared time interval: {}", pixel.time_first);
        }
      }
    }
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::test_grain_spill_slicer)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::test_grain_spill_slicer)
