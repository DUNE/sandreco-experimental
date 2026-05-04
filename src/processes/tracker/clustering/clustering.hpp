#include <edep_reader/edep_reader.hpp>
#include <geoinfo/drift_info.hpp>
#include <geoinfo/geoinfo.hpp>
#include <geoinfo/stt_info.hpp>
#include <geoinfo/tracker_info.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>
#include <tracker/digi.h>
#include <tracker/cluster_container.h>
#include <cluster_analyzer.hpp>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

namespace sand::tracker {

  class ClusterAnalyzer;
  class clustering : public ufw::process {
    public:
      clustering();
      void configure(const ufw::config& cfg) override;
      void run() override;
  
    private:
      std::map<const geoinfo::tracker_info::station *, std::vector<digi::signal>> group_signals_by_station();
      void clusterize_signals(const std::vector<digi::signal> & signals);
      void build_cluster(cluster_container::cluster<>& cluster,
                   const std::vector<digi::signal>& signals,
                   std::vector<bool>& visited,
                   size_t index,
                   const geoinfo& gi);
     std::unique_ptr<ClusterAnalyzer> cluster_analyzer_;


  };

} // namespace sand::tracker

UFW_REGISTER_PROCESS(sand::tracker::clustering)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::tracker::clustering)