#include <grain/digi.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

namespace sand::test {

  class test_grain_detector_response : public ufw::process {
   public:
    test_grain_detector_response();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
  };

  void test_grain_detector_response::configure(const ufw::config& cfg) {
    UFW_DEBUG("test_grain_detector_response configured at: {}", fmt::ptr(this));
  }

  test_grain_detector_response::test_grain_detector_response() : process({{"digi", "sand::grain::digi"}}, {}) {
    UFW_INFO("Creating a test_grain_detector_response process at {}", fmt::ptr(this));
  }

  void test_grain_detector_response::run() {
    UFW_DEBUG("test_grain_detector_response run called with context_id: {}", ufw::context::current()->id());
    const auto& digi_in = get<sand::grain::digi>("digi");
    for (auto& signal : digi_in.signals) {
      UFW_ASSERT(signal.tdc() >= 0, "Non-physical time: {}", signal.tdc());
      UFW_ASSERT(signal.npe() >= 0, "Non-physical number of detected photons : {}", signal.npe());
    }
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::test_grain_detector_response)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::test_grain_detector_response)
