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

namespace sand::tracker {

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
    const auto& digi = get<sand::tracker::digi>("digi");
    const auto& tgm        = ufw::context::current()->instance<root_tgeomanager>();
    auto & digi_out = set<sand::tracker::digi>("digi");

    auto signals_by_station = group_signals_by_station();

    UFW_DEBUG(" STT subdetector implementation");

  }

  std::map<const geoinfo::tracker_info::station *, std::vector<digi::signal>> clustering::group_signals_by_station() {
    const auto& digi = get<sand::tracker::digi>("digi");
    const auto& gi   = get<geoinfo>();
    std::map<const geoinfo::tracker_info::station *, std::vector<digi::signal>> signals_by_station;

    for (const auto& signal : digi.signals) {
      const auto& chsignal = signal.channel();
      const auto w = gi.tracker().wire_at(chsignal);
      signals_by_station[w.parent].push_back(signal);
    }

    for (const auto &[station, signals] : signals_by_station) {
      UFW_DEBUG("Found {} signals for station corresponding to daq_link {}.", signals.size(), station->daq_link);
    }
    return signals_by_station;
    
  }

} // namespace sand::tracker