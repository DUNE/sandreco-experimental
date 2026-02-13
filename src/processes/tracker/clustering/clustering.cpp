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
    auto clusters_by_station = clusterize_signals_by_stations(signals_by_station);
    for(const auto & [station, clusters] : clusters_by_station) {
      UFW_INFO("Found {} clusters for station {}.", clusters.size(), station->daq_link);
      for (const auto & cluster : clusters) {
        UFW_INFO("Found cluster with {} signals.", cluster.size());
      }     
    }


    // for (const auto& signal : digi.signals) {
    //   tracker::digi::signal clustered_signal = signal; //TODO cluster signals
    //   auto & hits = clustered_signal.true_hits();
    //   UFW_DEBUG("Clustering signal with TDC {} and ADC {}.", static_cast<int>(signal.channel().channel), signal.tdc, signal.adc);
    //   const auto & chsignal = clustered_signal.channel();
    //   UFW_DEBUG("  Channel info: subdetector {}, link {}, channel {}.", 
    //             static_cast<int>(chsignal.subdetector), static_cast<int>(chsignal.link), static_cast<int>(chsignal.channel));
    //   const auto w = gi.tracker().wire_at(chsignal);
    //   UFW_DEBUG("Corresponding to wire with info: head {}, tail {}", w.head, w.tail);
    //   digi_out.signals.push_back(clustered_signal);
    // }

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

  std::map<const geoinfo::tracker_info::station *, std::vector<std::vector<digi::signal>>> clustering::clusterize_signals_by_stations(const std::map<const geoinfo::tracker_info::station *, std::vector<digi::signal>> & signals_by_station) {
    std::map<const geoinfo::tracker_info::station *, std::vector<std::vector<digi::signal>>> clusters_by_station;
    for (const auto& [station, signals] : signals_by_station) {
      std::vector<std::vector<digi::signal>> clusters_in_station;
      std::for_each(signals.begin(), signals.end(), [&](const auto& signal) {
        std::vector<digi::signal> current_cluster = {signal};
        clusterize_signals(current_cluster, signals, clusters_in_station);
      });
      clusters_by_station[station] = clusters_in_station;
    }
    return clusters_by_station;
  }

  void clustering::clusterize_signals(std::vector<digi::signal> & current_cluster, const std::vector<digi::signal> & signals, std::vector<std::vector<digi::signal>> & clusters) {
    for (const auto& signal : signals) {
      if (std::find(current_cluster.begin(), current_cluster.end(), signal) != current_cluster.end()) {
        continue;
      }
      current_cluster.push_back(signal); ///TODO cluster signals
    }

    if (clusters.size() > 0) {
      if(!std::is_permutation(current_cluster.begin(), current_cluster.end(), clusters[clusters.size()-1].begin(), clusters[clusters.size()-1].end())) clusters.push_back(current_cluster);
    } else {
      clusters = {current_cluster};
    }

    ///TO-DO: for now just add all signals to a cluster and call it done, 
    ///real recursive algorithm to be implemented

  }

// void sand_reco::tracker::ClustersByProximity::clusterize(const std::vector<sand_reco::tracker::DigitID>& digits)
// {
//   if (digits.size() > 0) {
//     std::map<sand_geometry::tracker::CellID, sand_reco::tracker::DigitID> fMap;
//     std::for_each(digits.begin(), digits.end(), [&fMap](const sand_reco::tracker::DigitID &d) {
//       fMap[sand_geometry::tracker::CellID(d())] = d;
//     });

//     // To Do: should be a a config paramenter
//     int cluster_size = 3;
//     for (auto it = fMap.begin(); it != fMap.end(); it++) {
//       std::vector<sand_reco::tracker::DigitID> current_cluster = {it->second};
//       findCluster(current_cluster, it, fMap, cluster_size);
//     }
//   }
// }

// void sand_reco::tracker::ClustersByProximity::findCluster(std::vector<sand_reco::tracker::DigitID>& current_cluster, 
//                                                  std::map<sand_geometry::tracker::CellID, sand_reco::tracker::DigitID>::iterator it, 
//                                                  std::map<sand_geometry::tracker::CellID, sand_reco::tracker::DigitID>& fMap, 
//                                                  int cluster_size) {

//     for (auto next_it = fMap.begin(); next_it != fMap.end(); next_it++) {
      
//       const auto& next_cell = getSandGeoManager()->getCellInfo(next_it->first);
//       if (std::find(current_cluster.begin(), current_cluster.end(), next_it->first) 
//             != current_cluster.end()) {
//         continue;
//       }

//       int adjacent_count = 0;
//       for (const auto& digit : current_cluster) {
//         const auto& cluster_cell = getSandGeoManager()->getCellInfo(sand_geometry::tracker::CellID(digit()));

//         if (cluster_cell->second.isAdjacent(next_cell->first)) {
//           adjacent_count++;
//         }
//       }
//       if (adjacent_count == 0) {
//         continue;
//       }

//       current_cluster.push_back(next_it->second);

//       if ((int)current_cluster.size() == cluster_size) {
//         if (!isPermutation(current_cluster)) {
//           addCluster(sand_reco::tracker::Cluster(getSandGeoManager(), current_cluster));
//         }
//         current_cluster.pop_back();
//       } else {
//         findCluster(current_cluster, next_it, fMap, cluster_size);
//       }
//     }
//     current_cluster.pop_back();
// }

 

} // namespace sand::tracker