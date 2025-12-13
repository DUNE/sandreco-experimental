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
namespace sand::stt {

  class fast_digi : public ufw::process {
   public:
    fast_digi();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    std::map<geo_id, std::vector<EDEPHit>> group_hits_by_tube();

    void digitize_hits_in_tubes(const std::map<geo_id, std::vector<EDEPHit>>& hits_by_tube);

    tracker::digi::signal create_signal(double wire_time, double edep_total, const channel_id& channel);

    std::optional<tracker::digi::signal> process_hits_for_wire(const std::vector<EDEPHit>& hits,
                                                               const sand::geoinfo::stt_info::wire& wire,
                                                               const geo_id& tube_id);

   private:
    double m_drift_velocity; //[mm/ns]
    double m_wire_velocity;  //[mm/ns]
    double m_sigma_tdc;      //[ns]
  };

} // namespace sand::stt

UFW_REGISTER_PROCESS(sand::stt::fast_digi)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::stt::fast_digi)
