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
#include <tracker/cluster_container.h>

#include <clustering.hpp>

namespace sand::tracker {

  void clustering::configure(const ufw::config& cfg) {
    process::configure(cfg);
  }

  clustering::clustering() : process({{"digi", "sand::tracker::digi"}}, {{"clu", "sand::tracker::cluster_container"}}) {
    UFW_DEBUG("Creating a clustering process at {}", fmt::ptr(this));
  }

  void clustering::run() {
    UFW_DEBUG("Running clustering process at {}", fmt::ptr(this));
    const auto& tree = get<sand::edep_reader>();
    const auto& gi   = get<geoinfo>();
    const auto& digi = get<sand::tracker::digi>("digi");
    const auto& tgm        = ufw::context::current()->instance<root_tgeomanager>();
    auto & clu_out = set<sand::tracker::cluster_container>("clu");

    auto signals_by_station = group_signals_by_station();
    for (const auto &[station, signals] : signals_by_station) {
      UFW_DEBUG("Clusterizing {} signals for station corresponding to daq_link {}.", signals.size(), station->daq_link);
      clusterize_signals(signals);
    }

    UFW_DEBUG(" Completed clustering process at {}", fmt::ptr(this));

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

  void clustering::clusterize_signals(const std::vector<digi::signal> & signals) {

    if(signals.empty()) return;
    auto & clu_out = set<sand::tracker::cluster_container>("clu");
    for (const auto & signal : signals) {
      cluster_container::cluster current_cluster(std::make_shared<digi::signal>(signal));
      build_cluster(current_cluster, signals);
    }
  } 

  void clustering::build_cluster(cluster_container::cluster & cluster, const std::vector<digi::signal> & signals) {
    /// TODO: implement clustering
    return;
  }

} // namespace sand::tracker