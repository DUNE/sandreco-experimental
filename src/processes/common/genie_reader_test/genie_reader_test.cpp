#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <genie_reader/genie_reader.hpp>
// #include <edep_reader/edep_reader.hpp>

namespace sand::common {

  class genie_reader_test : public ufw::process {
   public:
    genie_reader_test();
    void configure(const ufw::config& cfg) override;
    void run() override;
  };

  void genie_reader_test::configure(const ufw::config& cfg) {
    process::configure(cfg);
    UFW_INFO("Configuring genie_reader_test at {}.", fmt::ptr(this));
  }

  genie_reader_test::genie_reader_test() : process({}, {}) {
    UFW_INFO("Creating a genie_reader_test process at {}.", fmt::ptr(this));
  }

  void genie_reader_test::run() {
    const auto& reader     = get<sand::genie_reader>();
    const auto& events     = reader.events_;
    const auto& stdHeps    = reader.stdHeps_;
    const auto& nuParents  = reader.nuParents_;
    const auto& numiFluxes = reader.numiFluxes_;
    for (std::size_t i = 0; i < events.size(); i++) {
      const auto& event  = events[i];
      const auto& stdHep = stdHeps[i];
      UFW_INFO("Parsing spill entry number: {}", i);

      UFW_INFO("Event num {}", event.EvtNum_);
      UFW_INFO("Event vertex {}, {}, {}, {}.", event.EvtVtx_[0], event.EvtVtx_[1], event.EvtVtx_[2], event.EvtVtx_[3]);
      UFW_INFO("Event code {}.", event.EvtCode_);

      UFW_INFO("StdHep N {}.", stdHep.N_);
      UFW_INFO("StdHep P4 (p, E) {}, {}, {}, {}.", stdHep.P4_[0].Px(), stdHep.P4_[0].Py(), stdHep.P4_[0].Pz(),
               stdHep.P4_[0].E());

      if (numiFluxes) {
        const auto& numiFlux = numiFluxes.value()[i];
        UFW_INFO("NumiFlux Run {}.", numiFlux.Run_);
        UFW_INFO("NumiFlux Ndxdz {}.", numiFlux.Ndxdz_);
      } else {
        UFW_DEBUG("Invalid NumiFlux.");
      }

      if (nuParents) {
        const auto& nuParent = nuParents.value()[i];
        UFW_INFO("NuParent Pdg {}.", nuParent.Pdg_);
        UFW_INFO("NuParent DecMode {}.", nuParent.DecMode_);
      } else {
        UFW_DEBUG("Invalid NuParent.");
      }
    }
  }
} // namespace sand::common

UFW_REGISTER_PROCESS(sand::common::genie_reader_test)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::common::genie_reader_test)
