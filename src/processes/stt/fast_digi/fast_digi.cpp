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

#include <fast_digi.hpp>

namespace sand::stt {

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

    UFW_DEBUG(" STT subdetector implementation");
    std::map<geo_id, std::vector<EDEPHit>> hits_by_tube = group_hits_by_tube();
    digitize_hits_in_tubes(hits_by_tube);
  }

  std::map<geo_id, std::vector<EDEPHit>> fast_digi::group_hits_by_tube() {
    const auto& gi   = get<geoinfo>();
    const auto& tree = get<sand::edep_reader>();
    auto& tgm        = ufw::context::current()->instance<root_tgeomanager>();
    std::map<geo_id, std::vector<EDEPHit>> hits_by_tube;
    const auto* stt = dynamic_cast<const sand::geoinfo::stt_info*>(&gi.tracker());

    for (const auto& trj : tree) {
      const auto& hit_map = trj.GetHitMap(); // pointer, not value
      if (hit_map.find(component::STRAW) == hit_map.end())
        continue;

      UFW_DEBUG("Found {} STRAW hits for trajectory with ID {}", hit_map.at(component::STRAW).size(), trj.GetId());

      for (const auto& hit : hit_map.at(component::STRAW)) {
        pos_3d hit_mid_point =
            pos_3d((hit.GetStart().X() + hit.GetStop().X()) * 0.5, (hit.GetStart().Y() + hit.GetStop().Y()) * 0.5,
                   (hit.GetStart().Z() + hit.GetStop().Z()) * 0.5);

        tgm.navigator()->FindNode(hit_mid_point.x(), hit_mid_point.y(), hit_mid_point.z());

        geo_path node_path(tgm.navigator()->GetPath());
        geo_path partial_path = stt->partial_path(node_path, gi);
        geo_id ID             = stt->id(partial_path);
        hits_by_tube[ID].push_back(hit);

        UFW_DEBUG("Hit at position {} is in geometry node {}.", hit_mid_point, partial_path);
        UFW_DEBUG("Corresponding geo_id: {}", ID);
      }
    }

    return hits_by_tube;
  }

  void fast_digi::digitize_hits_in_tubes(const std::map<geo_id, std::vector<EDEPHit>>& hits_by_tube) {
    const auto& gi  = get<geoinfo>();
    auto& digi      = set<sand::tracker::digi>("digi");
    const auto* stt = dynamic_cast<const sand::geoinfo::stt_info*>(&gi.tracker());

    if (!stt)
      return;

    for (const auto& [tube_id, hits] : hits_by_tube) {
      const auto* wire = stt->get_wire_by_id(tube_id);

      if (!wire) {
        UFW_WARN("No wire found for geo_id {}", tube_id);
        continue;
      }

      UFW_DEBUG("Digitizing hits for tube {}", tube_id);

      auto signal = process_hits_for_wire(hits, *wire);
      if (signal) {
        digi.signals.emplace_back(std::move(*signal));
        std::for_each(hits.begin(), hits.end(), [&](const auto& hit) { digi.insert(hit.GetId()); });
      }
    }
  }

  tracker::digi::signal fast_digi::create_signal(double wire_time, double edep_total, const channel_id& channel) {
    tracker::digi::signal signal;
    std::normal_distribution<double> gaussian_error(0.0, m_sigma_tdc);
    auto ran       = gaussian_error(random_engine());
    signal.tdc     = wire_time + ran;
    signal.adc     = edep_total;
    signal.channel = channel;

    UFW_DEBUG("  Created signal: Channel(subdetector {}, channel {}), TDC = {}, ADC = {}",
              static_cast<int>(signal.channel.subdetector), static_cast<int>(signal.channel.channel), signal.tdc,
              signal.adc);

    return signal;
  }

  std::optional<tracker::digi::signal> fast_digi::process_hits_for_wire(const std::vector<EDEPHit>& hits,
                                                                        const sand::geoinfo::stt_info::wire& wire) {
    const auto& gi     = get<geoinfo>();
    const auto* stt    = dynamic_cast<const sand::geoinfo::stt_info*>(&gi.tracker());
    double wire_time   = std::numeric_limits<double>::max();
    double drift_time  = std::numeric_limits<double>::max();
    double signal_time = std::numeric_limits<double>::max();
    double t_hit       = std::numeric_limits<double>::max();
    double edep_total  = 0.0;

    for (const auto& hit : hits) {
      UFW_DEBUG("  Hit ID {}: Energy Deposit = {}, Start Position = {}, Stop Position = {}", hit.GetId(),
                hit.GetEnergyDeposit(), vec_4d(hit.GetStart()), vec_4d(hit.GetStop()));

      auto closest_points = stt->closest_points(
          vec_4d(hit.GetStart().X(), hit.GetStart().Y(), hit.GetStart().Z(), hit.GetStart().T()),
          vec_4d(hit.GetStop().X(), hit.GetStop().Y(), hit.GetStop().Z(), hit.GetStop().T()), m_drift_velocity, wire);

      const vec_4d& closest_point_hit  = closest_points.first;
      const vec_4d& closest_point_wire = closest_points.second;

      // Update timing parameters directly here
      double hit_smallest_time = stt->get_min_time(closest_point_hit, m_wire_velocity, wire);

      if (hit_smallest_time < wire_time) {
        wire_time   = hit_smallest_time;
        t_hit       = closest_point_hit.T();
        drift_time  = closest_point_wire.T() - t_hit;
        signal_time = hit_smallest_time - closest_point_wire.T();

        UFW_DEBUG("    Closest point on hit: {}", closest_point_hit);
        UFW_DEBUG("    Closest point on wire: {}", closest_point_wire);
      }
      edep_total += hit.GetEnergyDeposit();
    }

    if (wire_time == std::numeric_limits<double>::max()) {
      return std::nullopt;
    }

    return create_signal(wire_time, edep_total, wire.channel);
  }

} // namespace sand::stt
