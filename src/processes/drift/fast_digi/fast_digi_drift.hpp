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
                                                                      const geoinfo::tracker_info::wire_list&,
                                                                      const EDEPHit&);

      double calculate_wire_boundary_transverse(const geoinfo::tracker_info::wire*, 
                                                const geoinfo::tracker_info::wire*,
                                                const xform_3d&,
                                                double transverse_start,
                                                double transverse_end,
                                                size_t wire_index) const;

      std::pair<pos_3d, pos_3d> interpolate_segment_endpoint(const pos_3d& start_local,
                                          double dx_local, double dy_local, double dz_local,
                                          double segment_end_transverse,
                                          const xform_3d& transform) const;

      EDEPHit create_segment_hit(const pos_3d& segment_start_global,
                                 const pos_3d& segment_end_global,
                                 double segment_start_time,
                                 double time_direction,
                                 double total_time_span,
                                 double segment_fraction,
                                 const EDEPHit& original_hit) const;

      void log_segment_debug(const pos_3d& segment_start_global,
                            const pos_3d& segment_end_global,
                            const pos_3d& segment_end_local,
                            double segment_length,
                            double segment_fraction,
                            size_t wire_index) const;

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
