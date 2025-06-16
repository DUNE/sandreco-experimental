#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <common/sand.h>
#include <geoinfo/geoinfo.hpp>
#include <geoinfo/grain_info.hpp>
#include <geoinfo/ecal_info.hpp>
#include <geoinfo/tracker_info.hpp>

namespace sand::common {

  class geoinfo_test : public ufw::process {

  public:
    geoinfo_test();
    void configure (const ufw::config& cfg) override;
    void run() override;
    
  };

  void geoinfo_test::configure (const ufw::config& cfg) {
    process::configure(cfg);
    UFW_INFO("Configuring geoinfo_test at {}.", fmt::ptr(this));
  }

  geoinfo_test::geoinfo_test() : process({}, {}) {
    UFW_INFO("Creating a geoinfo_test process at {}.", fmt::ptr(this));
  }

  void geoinfo_test::run() {
    const auto& gi = instance<geoinfo>();
    UFW_INFO("Running a geoinfo_test process at {}."), fmt::ptr(this);
    UFW_INFO("GRAIN path: '{}'", gi.grain().path());
    UFW_INFO("ECAL path: '{}'", gi.ecal().path());
    UFW_INFO("TRACKER path: '{}'", gi.tracker().path());
  }

}

UFW_REGISTER_PROCESS(sand::common::geoinfo_test)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::common::geoinfo_test)
