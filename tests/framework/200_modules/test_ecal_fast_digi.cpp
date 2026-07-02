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
    double m_int_time_window;
  };

  void test_ecal_fast_digi::configure(const ufw::config& cfg) {
    UFW_DEBUG("test_ecal_fast_digi configured at: {}", fmt::ptr(this));
    m_int_time_window = cfg.at("int_time_window");
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

      // Check that ADC value matches the number of photo-electrons in the pulse
      // (since current implementation uses PE count directly as ADC)
      UFW_ASSERT(signal.adc() >= signal.true_hits().size(), "ADC value smaller than number of contributing PEs");

      // Check ToT is NaN (as per current implementation)
      UFW_ASSERT(std::isnan(signal.tot()), "ToT value is not NaN: {}", signal.tot());

      if (!signal.true_hits().empty()) {
        UFW_ASSERT(signal.data_source() == sand::ecal::digits_container::digit::source::sim,
                   "Signal with truth hits is not marked as simulation source");

        for (auto& pe : signal.true_hits()) {
          UFW_ASSERT(pe, "Signal truth channel is invalid");
          UFW_ASSERT(pe.arrival_time >= 0, "Photo-electron has non-physical arrival time: {}", pe.arrival_time);
          if ((pe.arrival_time <= signal.tdc() - m_int_time_window)
              && (pe.arrival_time >= signal.tdc() + m_int_time_window)) {
            UFW_WARN("Photo-electron arrival time is outside TDC time +- integration time window: {} not in [{}, {}]",
                     pe.arrival_time, signal.tdc() - m_int_time_window, signal.tdc() + m_int_time_window);
          }
        }
      }
    }
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::test_ecal_fast_digi)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::test_ecal_fast_digi)