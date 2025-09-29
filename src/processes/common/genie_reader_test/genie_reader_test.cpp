#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <genie_reader/genie_reader.hpp>
// #include <edep_reader/edep_reader.hpp>

namespace sand::common {

  class genie_reader_test : public ufw::process {

  public:
    genie_reader_test();
    void configure (const ufw::config& cfg) override;
    void run() override;
  };

  void genie_reader_test::configure (const ufw::config& cfg) {
    process::configure(cfg);
    UFW_INFO("Configuring genie_reader_test at {}.", fmt::ptr(this));
  }

  genie_reader_test::genie_reader_test() : process({}, {}) {
    UFW_INFO("Creating a genie_reader_test process at {}.", fmt::ptr(this));
  }

  void genie_reader_test::run() {
    const auto& reader = get<sand::genie_reader>();
    auto event = reader.event_;
    auto stdHep = reader.stdHep_;
    auto numiFlux = reader.numiFlux_;
    UFW_INFO("Event num {}", event.EvtNum_);
    UFW_INFO("Event vertex {}, {}, {}, {}.", event.EvtVtx_[0], event.EvtVtx_[1],
                                             event.EvtVtx_[2], event.EvtVtx_[3]);
    UFW_INFO("Event code {}.", event.EvtCode_->String());
    
    UFW_INFO("StdHep N {}.", stdHep.N_);
    UFW_INFO("StdHep P4 {}, {}, {}, {}.", stdHep.P4_[0][0], stdHep.P4_[0][1], 
                                          stdHep.P4_[0][2], stdHep.P4_[0][3]);
    UFW_INFO("NumiFlux Run {}.", numiFlux.Run_);
    UFW_INFO("NumiFlux Ndxdz {}.", numiFlux.Ndxdz_);

  }
}

UFW_REGISTER_PROCESS(sand::common::genie_reader_test)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::common::genie_reader_test)
