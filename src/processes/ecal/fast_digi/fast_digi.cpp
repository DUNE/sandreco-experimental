#include <fast_digi.hpp>
#include <geoinfo/ecal_info.hpp>
#include <ecal/pes.h>
#include <ecal/digi.h>

namespace sand::ecal {

  void fast_digi::configure(const ufw::config& cfg) { process::configure(cfg); }

  fast_digi::fast_digi() : process({{"pes", "sand::ecal::pes"}}, {{"digi", "sand::ecal::digi"}}) {
    UFW_DEBUG("Creating a ecal fast digitization process at {}", fmt::ptr(this));
  }

  void fast_digi::run() {
    UFW_DEBUG("Running a ecal fast digitization process at {}", fmt::ptr(this));
    const auto& gecal = get<geoinfo>().ecal();
    auto& pes         = get<sand::ecal::pes>("pes");
    auto& digi        = set<sand::ecal::digi>("digi");
  }

} // namespace sand::ecal