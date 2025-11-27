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

#include <fast_digi_drift.hpp>

namespace sand::drift {

  void fast_digi::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_drift_velocity = cfg.at("drift_velocity");
    m_wire_velocity  = cfg.at("wire_velocity");
    m_sigma_tdc      = cfg.at("sigma_tdc");
  }

  fast_digi::fast_digi() : process({}, {{"digi", "sand::tracker::digi"}}) {
    UFW_DEBUG("Creating a fast_digi process at {}", fmt::ptr(this));
  }

  void fast_digi::run() {
    UFW_DEBUG("Running fast_digi process at {}", fmt::ptr(this));
    const auto& tree = get<sand::edep_reader>();
    const auto& gi   = get<geoinfo>();
    auto& digi       = set<sand::tracker::digi>("digi");
    auto& tgm        = ufw::context::current()->instance<root_tgeomanager>();
    std::map<geo_id, std::vector<EDEPHit>> hits_by_tube = group_hits_by_station();

    UFW_DEBUG(" DRIFT subdetector implementation");
  }

  std::map<geo_id, std::vector<EDEPHit>> fast_digi::group_hits_by_station() {
    const auto& gi   = get<geoinfo>();
    const auto& tree = get<sand::edep_reader>();
    auto& tgm        = ufw::context::current()->instance<root_tgeomanager>();
    std::map<geo_id, std::vector<EDEPHit>> hits_by_wire;
    const auto* drift = dynamic_cast<const sand::geoinfo::drift_info*>(&gi.tracker());

    for (const auto& trj : tree) {
      const auto& hit_map = trj.GetHitMap(); // pointer, not value
      if (!trj.HasHitInDetector(component::DRIFT)) 
        continue;

      UFW_DEBUG("Found {} DRIFT hits for trajectory with ID {}", hit_map.at(component::DRIFT).size(), trj.GetId());

      for(const auto& hit : hit_map.at(component::DRIFT)) {
        pos_3d hit_mid_point =
            pos_3d((hit.GetStart().X() + hit.GetStop().X()) * 0.5, (hit.GetStart().Y() + hit.GetStop().Y()) * 0.5,
                   (hit.GetStart().Z() + hit.GetStop().Z()) * 0.5);

        tgm.navigator()->FindNode(hit_mid_point.x(), hit_mid_point.y(), hit_mid_point.z());
        geo_path node_path(tgm.navigator()->GetPath());

        UFW_DEBUG(" DRIFT hit at position ({}, {}, {}) is in node path '{}'", hit_mid_point.x(), hit_mid_point.y(), hit_mid_point.z(), node_path);

        geo_path partial_path = drift->partial_path(node_path, gi);

        UFW_DEBUG(" Partial path for DRIFT hit: '{}'", partial_path);
        geo_id ID = drift->id(partial_path);

        // UFW_DEBUG("Corresponding geo_id: subdetector {}, supermodule {}, plane {}.",
        //           static_cast<int>(ID.drift.subdetector), static_cast<int>(ID.drift.supermodule),
        //           static_cast<int>(ID.drift.plane));
      }
      
    }

    return hits_by_wire;
  }

} // namespace sand::stt
