#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <edep_reader/edep_reader.hpp>
#include <geoinfo/drift_info.hpp>
#include <geoinfo/geoinfo.hpp>
#include <geoinfo/stt_info.hpp>
#include <geoinfo/tracker_info.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>
#include <tracker/digi.h>

#include <clustering.hpp>

namespace sand::stt {

  void clustering::configure(const ufw::config& cfg) {
    process::configure(cfg);
  }

  clustering::clustering() : process({{"digi", "sand::tracker::digi"}}, {{"digi", "sand::tracker::digi"}}) {
    UFW_DEBUG("Creating a clustering process at {}", fmt::ptr(this));
  }

  void clustering::run() {
    UFW_DEBUG("Running clustering process at {}", fmt::ptr(this));
    const auto& tree = get<sand::edep_reader>();
    const auto& gi   = get<geoinfo>();
    auto& digi       = set<sand::tracker::digi>("digi");
    auto& tgm        = ufw::context::current()->instance<root_tgeomanager>();

    UFW_DEBUG(" STT subdetector implementation");

  }

 

} // namespace sand::stt
