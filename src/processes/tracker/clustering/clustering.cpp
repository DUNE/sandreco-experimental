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
    UFW_INFO("Stations with signals : {}", signals_by_station.size());
    for (const auto &[station, signals] : signals_by_station) {
      UFW_DEBUG("Clusterizing {} signals for station corresponding to daq_link {}.", signals.size(), station->daq_link);
      clusterize_signals(signals);
      total_clusters += clu_out.clusters.size();
    }
    //Clustering process debug info
    const size_t nSignals = digi.signals.size();
    const size_t nClusters = clu_out.clusters.size();
    UFW_INFO("=========CLUSTER ANALYSIS=========");
    UFW_INFO("Total signals collected : {}", nSignals);
    UFW_INFO("Total clusters found : {}", nClusters);

    if(nClusters > 0 ){
      std::vector<size_t> cluster_size;
      cluster_size.reserve(nClusters);
      for(const auto& c : clu_out.clusters){
        cluster_size.push_back(c.digits().size());
      }

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
    auto & clu_out = set<sand::tracker::cluster_container>("clu"); //output clusters
    const auto& gi = get<geoinfo>();

    const size_t n = signals.size();
    std::vector<bool> clustered(n, false);

    for (size_t i = 0; i < n; ++i) {
      if (clustered[i]) continue;
      
      cluster_container::cluster current_cluster;
      build_cluster(current_cluster, signals, clustered, i);
      if(!current_cluster.digits().empty()){
      clu_out.clusters.push_back(std::move(current_cluster));
      }
    }
  } 

    void clustering::build_cluster(cluster_container::cluster& cluster,const std::vector<digi::signal>& signals, std::vector<bool>& visited, size_t index)
    {
        const auto& gi = get<geoinfo>();
        visited[index] = true;
       cluster.add_digit(index);

        const auto& wire_i = gi.tracker().wire_at(signals[index].channel());

        for (size_t j = 0; j < signals.size(); ++j)
        {
            if (visited[j]) continue;
            const auto& wire_j = gi.tracker().wire_at(signals[j].channel());
            if (wire_i.is_adjecent(wire_j))
            {
                build_cluster(cluster, signals, visited, j);
            }
        }
    }

} // namespace sand::tracker