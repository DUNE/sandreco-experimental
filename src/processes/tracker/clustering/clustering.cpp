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

#include <cluster_analyzer.hpp>
#include <clustering.hpp>

namespace {
/**
 * @brief Get the view ID from a channel ID. The view ID is determined by the upper 16 bits of the channel number.
 * @param chid The channel ID to extract the view ID from.
 * @return The view ID corresponding to the given channel ID.
 */

inline int getViewID(const sand::channel_id& chid)
{
    return static_cast<int>(chid.channel >> 16);
}

}

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
    cluster_analyzer_ = std::make_unique<ClusterAnalyzer>();
    
}


void clustering::run()
{
    const auto& digi = get<sand::tracker::digi>("digi");
    const auto& gi   = get<geoinfo>();

    auto& clu_out = set<sand::tracker::cluster_container>("clu");

    auto signals_by_station = group_signals_by_station();

    for (const auto& [station, signals] : signals_by_station)
        clusterize_signals(signals);

    if (cluster_analyzer_)
    {
        const auto& digi = get<sand::tracker::digi>("digi");
        const auto& gi   = get<geoinfo>();
        cluster_analyzer_->analyzeEvent(digi, clu_out, gi);
    }
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
    std::vector<bool> visited(n, false);

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
    cluster_container::cluster& cluster,
    const std::vector<digi::signal>& signals,
    std::vector<bool>& visited,
    size_t index,
    const geoinfo& gi)
{
    visited[index] = true;

    cluster.add_digit(index);

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

} // namespace sand::tracker
