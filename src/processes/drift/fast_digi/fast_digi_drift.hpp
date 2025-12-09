#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <edep_reader/edep_reader.hpp>
#include <geoinfo/drift_info.hpp>
#include <geoinfo/geoinfo.hpp>
#include <geoinfo/drift_info.hpp>
#include <geoinfo/tracker_info.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>
#include <tracker/digi.h>
namespace sand::drift {

  class fast_digi : public ufw::process {
    public:
      fast_digi();
      void configure(const ufw::config& cfg) override;
      void run() override;

    private:
      std::map<const geoinfo::tracker_info::wire *, std::vector<EDEPHit>> group_hits_by_wire();

      std::map<const geoinfo::tracker_info::wire *, EDEPHit> split_hit(size_t, size_t, 
                                                                      geoinfo::tracker_info::wire_list,
                                                                      const EDEPHit&);

      void digitize_hits_in_wires(const std::map<const geoinfo::tracker_info::wire *, std::vector<EDEPHit>>&);

      tracker::digi::signal create_signal(double, double, const channel_id&);

      std::optional<tracker::digi::signal> process_hits_for_wire(const std::vector<EDEPHit>& ,
                                                  const sand::geoinfo::drift_info::wire&);

      

    private:
      double m_drift_velocity; //[mm/ns]
      double m_wire_velocity;  //[mm/ns]
      double m_sigma_tdc;      //[ns]
  };

} // namespace sand::stt

UFW_REGISTER_PROCESS(sand::drift::fast_digi)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::drift::fast_digi)
