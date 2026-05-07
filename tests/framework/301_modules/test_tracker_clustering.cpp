#include <common/version.h>
#include <ufw/config.hpp>
#include <ufw/context.hpp>
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

namespace sand::test {

  class test_tracker_clustering : public ufw::process {
   public:

    using cluster_container = sand::tracker::cluster_container;

    test_tracker_clustering();
    void configure(const ufw::config& cfg) override;
    void run() override;
    void analyze_cluster_container();
    void log_clusters_sizes();
    void log_clusters_distribution();
    void log_clusters_topology();
    void log_clusters_efficiency();

   private:
  };

  void test_tracker_clustering::configure(const ufw::config& cfg) {
    UFW_DEBUG("test_tracker_clustering configured at: {}", fmt::ptr(this));
  }

  test_tracker_clustering::test_tracker_clustering() : process({{"clu", "sand::tracker::cluster_container"}, {"digi", "sand::tracker::digi"}}, {}) {
    UFW_INFO("Creating a test_tracker_clustering process at {}", fmt::ptr(this));
  }

  void test_tracker_clustering::run() {
    UFW_DEBUG("test_tracker_clustering run called with context_id: {}", ufw::context::current()->id());
    analyze_cluster_container();
  }

void test_tracker_clustering::analyze_cluster_container()
    {
        const auto& digi = get<sand::tracker::digi>("digi");
        const auto& clu = get<sand::tracker::cluster_container>("clu");

        const size_t nSignals  = digi.signals.size();
        const size_t nClusters = clu.clusters.size();

        UFW_DEBUG("============== CLUSTER ANALYSIS ==============");
        UFW_DEBUG("Total signals  : {}", nSignals);
        UFW_DEBUG("Total clusters : {}", nClusters);

        if (nClusters == 0) {
            UFW_WARN("No clusters found in this event!");
            return;
        }

        log_clusters_sizes();
        log_clusters_distribution();
        log_clusters_topology();
        log_clusters_efficiency();

        UFW_DEBUG("===============================================");
    }

void test_tracker_clustering::log_clusters_sizes()
    {
        const auto& clu = get<sand::tracker::cluster_container>("clu");
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

void test_tracker_clustering::log_clusters_distribution()
    {
        const auto& clu = get<sand::tracker::cluster_container>("clu");
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

void test_tracker_clustering::log_clusters_topology()
    {
        const auto& clu = get<sand::tracker::cluster_container>("clu");
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

void test_tracker_clustering::log_clusters_efficiency()
    {
        const auto& clu = get<sand::tracker::cluster_container>("clu");
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
} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::test_tracker_clustering)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::test_tracker_clustering)
