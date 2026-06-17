#include <common/version.h>
#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <edep_reader/edep_reader.hpp>
#include <geoinfo/drift_info.hpp>
#include <geoinfo/geoinfo.hpp>
#include <geoinfo/stt_info.hpp>
#include <geoinfo/tracker_info.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>
#include <tracker/digi.h>
#include <tracker/cluster_container.h>

namespace sand::test {

  class test_stt_digi : public ufw::process {
   public:


    test_stt_digi();
    void configure(const ufw::config& cfg) override;
    void run() override;
    void analyze_stt_digi();

   private:
  };

  void test_stt_digi::configure(const ufw::config& cfg) {
    UFW_DEBUG("test_stt_digi configured at: {}", fmt::ptr(this));
  }

  test_stt_digi::test_stt_digi() : process({{"digi", "sand::tracker::digi"}}, {}) {
    UFW_INFO("Creating a test_stt_digi process at {}", fmt::ptr(this));
  }

  void test_stt_digi::run() {
    UFW_DEBUG("test_stt_digi run called with context_id: {}", ufw::context::current()->id());
    analyze_stt_digi();
  }

void test_stt_digi::analyze_stt_digi()
    {
        const auto& digi = get<sand::tracker::digi>("digi");

        const size_t nSignals  = digi.signals.size();

        UFW_DEBUG("============== DIGI ANALYSIS ==============");
        UFW_DEBUG("Just a mockup for now");
        UFW_DEBUG("Total signals : {}", nSignals);

        if (nSignals == 0) {
            UFW_WARN("No signals found in this event!");
            return;
        }

        UFW_DEBUG("===============================================");
    }


} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::test_stt_digi)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::test_stt_digi)
