#include <edep_reader/EDEPHit.h>
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
      UFW_ASSERT(std::isnan(signal.tot()), "Unused ToT is not NaN: {}", signal.tot());
      // if data comes from simulation check that there is truth associated with signal, and it is valid
      if (signal.data_source() == sand::grain::digi::signal::source::sim) {
        UFW_ASSERT(!signal.true_hits().empty(), "Simulated signal has no associated truth");
        for (auto& signal_truth : signal.true_hits()) {
          UFW_ASSERT(signal_truth, "Signal truth channel is invalid");
          UFW_DEBUG("Edep hit id: {}, with energy: {}, length: {}", signal_truth->GetId(),
                    signal_truth->GetSecondaryDeposit(), signal_truth->GetTrackLength());
        }
      }
      // if data comes from detector verify truth is empty
      else if (signal.data_source() == sand::grain::digi::signal::source::det) {
        UFW_ASSERT(signal.true_hits().empty(), "Detected signal has associated mc truth");
      }
    }
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::test_grain_detector_response)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::test_grain_detector_response)
