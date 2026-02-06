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
namespace sand::tracker {

  class clustering : public ufw::process {
   public:
    clustering();
    void configure(const ufw::config& cfg) override;
    void run() override;


  };

} // namespace sand::tracker

UFW_REGISTER_PROCESS(sand::tracker::clustering)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::tracker::clustering)