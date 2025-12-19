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
        std::for_each(hits.begin(), hits.end(), [&signal](const auto& hit) { signal->insert(hit.GetId()); });
        digi.signals.emplace_back(std::move(*signal));
      }
    }
  }

  tracker::digi::signal fast_digi::create_signal(double wire_time, double edep_total, const channel_id& channel) {
    std::normal_distribution<double> gaussian_error(0.0, m_sigma_tdc); //FIXME should be member
    auto ran = gaussian_error(random_engine());
    //FIXME replace 200 with maximum drift + signal time
    reco::digi::time trange{wire_time - 200., wire_time, wire_time + 5. * m_sigma_tdc};
    tracker::digi::signal signal{reco::digi{channel, trange}, wire_time + ran, edep_total};

    UFW_DEBUG("  Created signal: Channel(subdetector {}, channel {}), TDC = {}, ADC = {}",
              static_cast<int>(signal.channel().subdetector), static_cast<int>(signal.channel().channel), signal.tdc,
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

      auto closest_points = closest_points_hit_wire(
          vec_4d(hit.GetStart().X(), hit.GetStart().Y(), hit.GetStart().Z(), hit.GetStart().T()),
          vec_4d(hit.GetStop().X(), hit.GetStop().Y(), hit.GetStop().Z(), hit.GetStop().T()), m_drift_velocity, wire);

      const vec_4d& closest_point_hit  = closest_points.first;
      const vec_4d& closest_point_wire = closest_points.second;

      // Update timing parameters directly here
      double hit_smallest_time = get_min_time(closest_point_hit, m_wire_velocity, wire);

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

  std::pair<vec_4d,vec_4d> fast_digi::closest_points_hit_wire(const vec_4d& hit_start, const vec_4d& hit_stop,  //TO-DO move to fast_digi
                                                            double v_drift, const geoinfo::tracker_info::wire& w) const {
    std::pair<vec_4d,vec_4d> closest_points;

    pos_3d start(hit_start.X(), hit_start.Y(), hit_start.Z());
    pos_3d stop(hit_stop.X(), hit_stop.Y(), hit_stop.Z());

    auto seg_params = w.closest_approach_segment(start, stop);

    std::vector<vec_4d> result;

    double& t       = seg_params.first; // Parameter along s
    double& t_prime = seg_params.second; // Parameter along r

    // Calculate the closest point on the line segment
    pos_3d closest_point_hit = start + (stop - start) * t;

    if (t == 0 || t == 1) {
      dir_3d AP = closest_point_hit - w.head;
      t_prime   = AP.Dot(w.direction()) / w.direction().Mag2();
      t_prime   = std::max(0.0, std::min(1.0, t_prime));
    }

    // Calculate the corresponding point on the wire
    pos_3d closest_point_wire = w.head + w.direction() * t_prime;

    double fraction = sqrt((closest_point_hit - start).Mag2() / (stop - start).Mag2());
    vec_4d closest_point_hit_l(closest_point_hit.X(), closest_point_hit.Y(), closest_point_hit.Z(),
                                hit_start.T() + fraction * (hit_stop.T() - hit_start.T()));

    closest_points.first = closest_point_hit_l;

    vec_4d closest_point_wire_l(closest_point_wire.X(), closest_point_wire.Y(), closest_point_wire.Z(),
                                closest_point_hit_l.T()
                                    + sqrt((closest_point_hit - closest_point_wire).Mag2()) / v_drift);

    closest_points.second = closest_point_wire_l;

    return closest_points;
  }

  double fast_digi::get_min_time(const vec_4d& point, double v_signal_inwire, const geoinfo::tracker_info::wire& w) const {
    return point.T() + sqrt((pos_3d(point.Vect()) - w.head).Mag2()) / v_signal_inwire;
  }

} // namespace sand::stt
