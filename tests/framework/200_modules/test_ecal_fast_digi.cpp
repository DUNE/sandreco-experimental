#include <ecal/digit.h>
#include <ecal/photo_electron.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

namespace sand::test {

  class test_ecal_fast_digi : public ufw::process {
   public:
    test_ecal_fast_digi();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
  };

  void test_ecal_fast_digi::configure(const ufw::config& cfg) {
    UFW_DEBUG("test_ecal_fast_digi configured at: {}", fmt::ptr(this));
  }

  test_ecal_fast_digi::test_ecal_fast_digi() : process({{"digi", "sand::ecal::digits_container"}}, {}) {
    UFW_INFO("Creating a test_ecal_fast_digi process at {}", fmt::ptr(this));
  }

  void test_ecal_fast_digi::run() {
    UFW_DEBUG("test_ecal_fast_digi run called with context_id: {}", ufw::context::current()->id());
    const auto& digi_in = get<sand::ecal::digits_container>("digi");

    for (auto& signal : digi_in.digits) {
      // Check basic signal properties
      UFW_ASSERT(signal.tdc() >= 0, "Non-physical TDC time: {}", signal.tdc());
      UFW_ASSERT(signal.adc() >= 0, "Non-physical ADC value: {}", signal.adc());
      UFW_ASSERT(signal.t(), "Signal time window is invalid (NaN)");

      // Check time window properties (start, peak, end)
      UFW_ASSERT(signal.t().earliest() <= signal.t().best(), "Time window start > peak: {} > {}", signal.t().earliest(),
                 signal.t().best());
      UFW_ASSERT(signal.t().best() <= signal.t().latest(), "Time window peak > end: {} > {}", signal.t().best(),
                 signal.t().latest());

      // Check that time window is valid (non-degenerate)
      UFW_ASSERT(signal.t().earliest() < signal.t().latest(), "Time window is degenerate: {} >= {}",
                 signal.t().earliest(), signal.t().latest());

      // Check that ADC value matches the number of photo-electrons in the pulse
      // (since current implementation uses PE count directly as ADC)
      UFW_ASSERT(signal.adc() >= signal.true_hits().size(), "ADC value smaller than number of contributing PEs");

      // Check ToT is NaN (as per current implementation)
      UFW_ASSERT(std::isnan(signal.tot()), "ToT value is not NaN: {}", signal.tot());

      // Check truth hits if available
      if (!signal.true_hits().empty()) {
        UFW_ASSERT(signal.data_source() == sand::ecal::digits_container::digit::source::sim,
                   "Signal with truth hits is not marked as simulation source");

        for (auto& pe : signal.true_hits()) {
          UFW_ASSERT(pe.arrival_time >= 0, "Photo-electron has non-physical arrival time: {}", pe.arrival_time);
          if (!signal.t().contains(pe.arrival_time)) {
            UFW_WARN("Photo-electron arrival time outside signal window: {} not in [{}, {}]", pe.arrival_time,
                     signal.t().earliest(), signal.t().latest());
          }
        }
      }
    }
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::test_ecal_fast_digi)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::test_ecal_fast_digi)