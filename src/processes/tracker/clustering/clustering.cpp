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
: process({{"digi", "sand::tracker::digi"}},
          {{"clu",  "sand::tracker::cluster_container"}})
{
    UFW_DEBUG("Creating clustering process at {}", fmt::ptr(this));
    //cluster_analyzer_ = std::make_unique<ClusterAnalyzer>();
    
}


void clustering::run()
{
    const auto& digi = get<sand::tracker::digi>("digi");
    const auto& gi   = get<geoinfo>();

    auto& clu_out = set<sand::tracker::cluster_container>("clu");

    auto signals_by_station = group_signals_by_station();

    for (const auto& [station, signals] : signals_by_station)
        clusterize_signals(signals);

    analyze_cluster_container(clu_out);
}

std::map<const geoinfo::tracker_info::station*,
         std::vector<digi::signal>>
clustering::group_signals_by_station()
{
    const auto& digi = get<sand::tracker::digi>("digi");
    const auto& gi   = get<geoinfo>();

    std::map<const geoinfo::tracker_info::station*,
             std::vector<digi::signal>> signals_by_station;

    for (const auto& signal : digi.signals)
    {
        const auto& wire =
            gi.tracker().wire_at(signal.channel());

        signals_by_station[wire.parent].push_back(signal);
    }

    return signals_by_station;
}


void clustering::clusterize_signals(
    const std::vector<digi::signal>& signals)
{
    if (signals.empty())
        return;

    auto& clu_out = set<sand::tracker::cluster_container>("clu");

    const auto& gi = get<geoinfo>();

    const size_t n = signals.size();
    std::vector<char> visited(n, false);

    for (size_t i = 0; i < n; ++i)
    {
        if (visited[i])
            continue;

        cluster_container::cluster current_cluster;

        build_cluster(current_cluster, signals, visited, i, gi);

        if (!current_cluster.digits().empty())
            clu_out.clusters.push_back(std::move(current_cluster));
    }
}


void clustering::build_cluster(
    cluster_container::cluster<>& cluster,
    const std::vector<digi::signal>& signals,
    std::vector<char>& visited,
    size_t index,
    const geoinfo& gi)
{
    visited[index] = true;

    cluster.add_digit(signals[index]);

    const auto& wire_i =
        gi.tracker().wire_at(signals[index].channel());

        UFW_DEBUG("Wire {} has {} adjacent wires",
          wire_i.daq_channel,
          wire_i.adjacent_wires.size());

    for (size_t j = 0; j < signals.size(); ++j)
    {
        if (visited[j])
            continue;

        const auto& wire_j =
            gi.tracker().wire_at(signals[j].channel());

        if (wire_i.is_adjacent(&wire_j))
        build_cluster(cluster, signals, visited, j, gi);
    }
 
}

void clustering::analyze_cluster_container(const cluster_container& clu)
    {
        const auto& digi = get<sand::tracker::digi>("digi");

        const size_t nSignals  = digi.signals.size();
        const size_t nClusters = clu.clusters.size();

        UFW_DEBUG("============== CLUSTER ANALYSIS ==============");
        UFW_DEBUG("Total signals  : {}", nSignals);
        UFW_DEBUG("Total clusters : {}", nClusters);

        if (nClusters == 0) {
            UFW_WARN("No clusters found in this event!");
            return;
        }

        log_clusters_sizes(clu);
        log_clusters_distribution(clu);
        log_clusters_topology(clu);
        log_clusters_efficiency(clu);

        UFW_DEBUG("===============================================");
    }

void clustering::log_clusters_sizes(const cluster_container& clu)
    {
        std::vector<size_t> sizes;
        sizes.reserve(clu.clusters.size());

        for (const auto& c : clu.clusters)
            sizes.push_back(c.digits().size());

        auto minmax = std::minmax_element(sizes.begin(), sizes.end());
        double avg =
            std::accumulate(sizes.begin(), sizes.end(), 0.0) / sizes.size();

        UFW_DEBUG("Smallest cluster size : {}", *minmax.first);
        UFW_DEBUG("Largest cluster size  : {}", *minmax.second);
        UFW_DEBUG("Average cluster size  : {:.2f}", avg);

        std::map<size_t,size_t> hist;
        for (auto s : sizes)
            hist[s]++;

        UFW_DEBUG("--- Cluster size distribution ---");
        for (auto [size,count] : hist)
            UFW_DEBUG("  size {} : {}", size, count);
    }

void clustering::log_clusters_distribution(const cluster_container& clu)
    {
        const auto& digi = get<sand::tracker::digi>("digi");
        const auto& gi   = get<geoinfo>();

        std::map<int,size_t> station_count;
        std::map<int,size_t> plane_count;

        for (const auto& c : clu.clusters)
        {
            std::set<int> stations;
            std::set<int> planes;

            for (auto idx : c.digits())
            {
                const auto& sig  = idx;
                const auto& wire =
                    gi.tracker().wire_at(sig.channel());

                stations.insert(wire.parent->daq_link);

                int plane = static_cast<int>(wire.daq_channel.channel >> 16);
                planes.insert(plane);
            }

            for (auto s : stations)
                station_count[s]++;

            for (auto p : planes)
                plane_count[p]++;
        }

        UFW_DEBUG("--- Clusters per station ---");
        for (auto [st,count] : station_count)
            UFW_DEBUG("  station {} : {}", st, count);

        UFW_DEBUG("--- Clusters per plane ---");
        for (auto [pl,count] : plane_count)
            UFW_DEBUG("  plane {} : {}", pl, count);
    }

void clustering::log_clusters_topology(const cluster_container& clu)
    {
        const auto& digi = get<sand::tracker::digi>("digi");
        const auto& gi   = get<geoinfo>();

        UFW_DEBUG("--- Cluster topology ---");

        size_t cid = 0;

        for (const auto& c : clu.clusters)
        {
            std::vector<uint32_t> wire_ids;

            for (auto idx : c.digits())
            {
                const auto& sig  = idx;
                const auto& wire =
                    gi.tracker().wire_at(sig.channel());

                wire_ids.push_back(
                    wire.daq_channel.channel & 0xFFFF
                );
            }

            std::sort(wire_ids.begin(), wire_ids.end());

            uint32_t span =
                wire_ids.empty() ? 0 :
                wire_ids.back() - wire_ids.front();

            UFW_DEBUG("Cluster {} : size={}, wire span={}",
                     cid,
                     c.digits().size(),
                     span);

            cid++;
        }
    }

void clustering::log_clusters_efficiency(const cluster_container& clu)
    {
        size_t single_hits = 0;

        for (const auto& c : clu.clusters)
            if (c.digits().size() == 1)
                single_hits++;

        UFW_DEBUG("Isolated clusters (size=1) : {}", single_hits);

        if (!clu.clusters.empty())
        {
            UFW_DEBUG("Fraction isolated : {:.3f}",
                     double(single_hits) /
                     clu.clusters.size());
        }
    }

} // namespace sand::tracker
