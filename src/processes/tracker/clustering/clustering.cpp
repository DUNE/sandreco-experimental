#include <common/version.h>
#include <clustering.hpp>
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

namespace sand::tracker {

void clustering::configure(const ufw::config& cfg)
{
    process::configure(cfg);
}

clustering::clustering()
: process({{"digi", "sand::tracker::digi_container"}},
          {{"clu",  "sand::tracker::cluster_container"}})
{
    UFW_DEBUG("Creating clustering process at {}", fmt::ptr(this));
    //cluster_analyzer_ = std::make_unique<ClusterAnalyzer>();
    
}


channel_id digi_channel(const digi_container::digi& d) { return d.signals.front().channel(); }

void clustering::run()
{
    auto& clu_out = set<sand::tracker::cluster_container>("clu");

    auto digis_by_station = group_digis_by_station();

    for (const auto& [station, digis] : digis_by_station)
        clusterize_digis(digis);

    //analyze_cluster_container(clu_out);
}

std::map<const geoinfo::tracker_info::station*,
         digi_container::digi_collection>
clustering::group_digis_by_station()
{
    const auto& digi = get<sand::tracker::digi_container>("digi");
    const auto& gi   = get<geoinfo>();

    std::map<const geoinfo::tracker_info::station*,
             digi_container::digi_collection> digis_by_station;

    for (const auto& d : digi.digis)
    {
        if (d.signals.empty()) {
            UFW_WARN("Skipping a digi with no signals: its channel is unknown");
            continue;
        }

        const auto& wire =
            gi.tracker().wire_at(digi_channel(d));

        digis_by_station[wire.parent].push_back(d);
    }

    return digis_by_station;
}


void clustering::clusterize_digis(
    const digi_container::digi_collection& digis)
{
    if (digis.empty())
        return;

    auto& clu_out = set<sand::tracker::cluster_container>("clu");

    const auto& gi = get<geoinfo>();

    const size_t n = digis.size();
    std::vector<char> visited(n, false);

    for (size_t i = 0; i < n; ++i)
    {
        if (visited[i])
            continue;

        cluster_container::cluster current_cluster;

        build_cluster(current_cluster, digis, visited, i, gi);

        if (!current_cluster.empty())
            clu_out.clusters.push_back(std::move(current_cluster));
    }
}


void clustering::build_cluster(
    cluster_container::cluster& cluster,
    const digi_container::digi_collection& digis,
    std::vector<char>& visited,
    size_t index,
    const geoinfo& gi)
{
    visited[index] = true;

    cluster.push_back(digis[index]);

    const auto& wire_i =
        gi.tracker().wire_at(digi_channel(digis[index]));

        UFW_DEBUG("Wire {} has {} adjacent wires",
          wire_i.daq_channel,
          wire_i.adjacent_wires.size());

    for (size_t j = 0; j < digis.size(); ++j)
    {
        if (visited[j])
            continue;

        const auto& wire_j =
            gi.tracker().wire_at(digi_channel(digis[j]));

        if (wire_i.is_adjacent(&wire_j))
        build_cluster(cluster, digis, visited, j, gi);
    }

}

} // namespace sand::tracker
