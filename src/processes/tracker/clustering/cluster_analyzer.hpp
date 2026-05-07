#pragma once

#include <tracker/cluster_container.h>
#include <tracker/digi.h>
#include <geoinfo/geoinfo.hpp>
#include <geoinfo/tracker_info.hpp>

#include <map>
#include <set>
#include <vector>
#include <numeric>
#include <algorithm>

namespace sand::tracker {

class ClusterAnalyzer {

public:

    static void analyzeEvent(
        const sand::tracker::digi& digi,
        const cluster_container& clu,
        const sand::geoinfo& gi)
    {
        const size_t nSignals  = digi.signals.size();
        const size_t nClusters = clu.clusters.size();

        UFW_INFO("============== CLUSTER ANALYSIS ==============");
        UFW_INFO("Total signals  : {}", nSignals);
        UFW_INFO("Total clusters : {}", nClusters);

        if (nClusters == 0) {
            UFW_WARN("No clusters found in this event!");
            return;
        }

        clusterSize(clu);
        clustersDistribution(clu, digi, gi);
        clusterTopology(clu, digi, gi);
        clusterEfficiency(clu);

        UFW_INFO("===============================================");
    }

private:

    // --------------------------------------------------

    static inline int extractPlane(const sand::channel_id& chid)
    {
        return static_cast<int>(chid.channel >> 16);
    }

    // --------------------------------------------------

    static void clusterSize(const cluster_container& clu)
    {
        std::vector<size_t> sizes;
        sizes.reserve(clu.clusters.size());

        for (const auto& c : clu.clusters)
            sizes.push_back(c.digits().size());

        auto minmax = std::minmax_element(sizes.begin(), sizes.end());
        double avg =
            std::accumulate(sizes.begin(), sizes.end(), 0.0) / sizes.size();

        UFW_INFO("Smallest cluster size : {}", *minmax.first);
        UFW_INFO("Largest cluster size  : {}", *minmax.second);
        UFW_INFO("Average cluster size  : {:.2f}", avg);

        std::map<size_t,size_t> hist;
        for (auto s : sizes)
            hist[s]++;

        UFW_INFO("--- Cluster size distribution ---");
        for (auto [size,count] : hist)
            UFW_INFO("  size {} : {}", size, count);
    }

    // --------------------------------------------------

    static void clustersDistribution(
        const cluster_container& clu,
        const sand::tracker::digi& digi,
        const sand::geoinfo& gi)
    {
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

                int plane = extractPlane(wire.daq_channel);
                planes.insert(plane);
            }

            for (auto s : stations)
                station_count[s]++;

            for (auto p : planes)
                plane_count[p]++;
        }

        UFW_INFO("--- Clusters per station ---");
        for (auto [st,count] : station_count)
            UFW_INFO("  station {} : {}", st, count);

        UFW_INFO("--- Clusters per plane ---");
        for (auto [pl,count] : plane_count)
            UFW_INFO("  plane {} : {}", pl, count);
    }

    // --------------------------------------------------

    static void clusterTopology(
        const cluster_container& clu,
        const sand::tracker::digi& digi,
        const sand::geoinfo& gi)
    {
        UFW_INFO("--- Cluster topology ---");

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

            UFW_INFO("Cluster {} : size={}, wire span={}",
                     cid,
                     c.digits().size(),
                     span);

            cid++;
        }
    }

    // --------------------------------------------------

    static void clusterEfficiency(
        const cluster_container& clu)
    {
        size_t single_hits = 0;

        for (const auto& c : clu.clusters)
            if (c.digits().size() == 1)
                single_hits++;

        UFW_INFO("Isolated clusters (size=1) : {}", single_hits);

        if (!clu.clusters.empty())
        {
            UFW_INFO("Fraction isolated : {:.3f}",
                     double(single_hits) /
                     clu.clusters.size());
        }
    }

};

} // namespace sand::tracker