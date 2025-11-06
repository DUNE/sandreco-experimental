#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <tracker/digi.h>
#include <edep_reader/edep_reader.hpp>
#include <geoinfo/geoinfo.hpp>
#include <geoinfo/tracker_info.hpp>
#include <geoinfo/stt_info.hpp>
#include <geoinfo/drift_info.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>

#include <fast_digi.hpp>


namespace sand::stt {

  void fast_digi::configure (const ufw::config& cfg) {
    process::configure(cfg); 
    m_drift_velocity = cfg.at("drift_velocity");
    m_wire_velocity = cfg.at("wire_velocity");
    m_sigma_tdc = cfg.at("sigma_tdc");
  }

  fast_digi::fast_digi() : process({}, {{"digi", "sand::tracker::digi"}}) {
    UFW_DEBUG("Creating a fast_digi process at {}", fmt::ptr(this));
  } 

  void fast_digi::run() {

    UFW_DEBUG("Running fast_digi process at {}", fmt::ptr(this));
    const auto& tree = get<sand::edep_reader>();
    const auto& gi = get<geoinfo>();
    auto& digi = set<sand::tracker::digi>("digi");
    auto& tgm = ufw::context::current()->instance<root_tgeomanager>();


    UFW_DEBUG(" STT subdetector implementation");
    std::map<geo_id, std::vector<EDEPHit>> hits_by_tube = group_hits_by_tube(); 
    digitize_hits_in_tubes(hits_by_tube); 
  }

  std::map<geo_id, std::vector<EDEPHit>> fast_digi::group_hits_by_tube() {
    
    const auto& gi = get<geoinfo>();
    const auto& tree = get<sand::edep_reader>();
    auto& tgm = ufw::context::current()->instance<root_tgeomanager>();
    std::map<geo_id, std::vector<EDEPHit>> hits_by_tube;
    const auto* stt = dynamic_cast<const sand::geoinfo::stt_info*>(&gi.tracker());

    for (const auto& trj : tree) {
        const auto& hit_map = trj.GetHitMap(); // pointer, not value    
        if(hit_map.find(component::STRAW) == hit_map.end()) continue;

        UFW_DEBUG("Found {} STRAW hits for trajectory with ID {}", hit_map.at(component::STRAW).size(), trj.GetId());
    
        for (const auto& hit : hit_map.at(component::STRAW)){

          pos_3d hit_mid_point = pos_3d((hit.GetStart().X() + hit.GetStop().X()) * 0.5,
                                        (hit.GetStart().Y() + hit.GetStop().Y()) * 0.5,
                                        (hit.GetStart().Z() + hit.GetStop().Z()) * 0.5);

          tgm.navigator()->FindNode(hit_mid_point.x(), hit_mid_point.y(), hit_mid_point.z());

          geo_path node_path(tgm.navigator()->GetPath());
          geo_path partial_path = stt->partial_path(node_path,gi); 
          geo_id ID = stt->id(partial_path); 
          hits_by_tube[ID].push_back(hit);

          UFW_DEBUG("Hit at position ({}, {}, {}) is in geometry node {}.", hit_mid_point.X(), hit_mid_point.Y(), hit_mid_point.Z(), partial_path);
          UFW_DEBUG("Corresponding geo_id: subdetector {}, supermodule {}, station {}, straw {}.",
                    static_cast<int>(ID.stt.subdetector),
                    static_cast<int>(ID.stt.supermodule),
                    static_cast<int>(ID.stt.plane),
                    static_cast<int>(ID.stt.tube));
          }
        }

    return hits_by_tube;
    
  }


  void fast_digi::digitize_hits_in_tubes(const std::map<geo_id, std::vector<EDEPHit>>& hits_by_tube) {

    const auto& gi = get<geoinfo>();
    auto& digi = set<sand::tracker::digi>("digi");
    const auto* stt = dynamic_cast<const sand::geoinfo::stt_info*>(&gi.tracker());

    if (!stt) return;

    for (const auto& [tube_id, hits] : hits_by_tube) {
        const auto* wire = stt->get_wire_by_id(tube_id);
        
        if (!wire) {
            log_tube_warning("No wire found", tube_id);
            continue;
        }

        log_tube_debug("Digitizing hits for tube", tube_id);
        
        auto signal = process_hits_for_wire(hits, *wire, tube_id);
        if (signal) {
            digi.signals.emplace_back(std::move(*signal));
            std::for_each(hits.begin(), hits.end(),
              [&](const auto &hit) { digi.add(hit.GetId()); });
        }
    }
  }

  void fast_digi::log_tube_warning(std::string_view message, const geo_id& tube_id) {
        UFW_WARN("{} for geo_id: subdetector {}, supermodule {}, station {}, straw {}.",
                message,
                static_cast<int>(tube_id.stt.subdetector),
                static_cast<int>(tube_id.stt.supermodule),
                static_cast<int>(tube_id.stt.plane),
                static_cast<int>(tube_id.stt.tube));
    }

  void fast_digi::log_tube_debug(std::string_view message, const geo_id& tube_id) {
        UFW_DEBUG("{}: subdetector {}, supermodule {}, station {}, straw {}.",
                message,
                static_cast<int>(tube_id.stt.subdetector),
                static_cast<int>(tube_id.stt.supermodule),
                static_cast<int>(tube_id.stt.plane),
                static_cast<int>(tube_id.stt.tube));
    }

  void fast_digi::log_hit_debug(const EDEPHit& hit) {
        UFW_DEBUG("  Hit ID {}: Energy Deposit = {}, Start Position = ({}, {}, {}, {}), Stop Position = ({}, {}, {}, {})",
                hit.GetId(),
                hit.GetEnergyDeposit(),
                hit.GetStart().X(), hit.GetStart().Y(), hit.GetStart().Z(), hit.GetStart().T(),
                hit.GetStop().X(), hit.GetStop().Y(), hit.GetStop().Z(), hit.GetStop().T());
  }

  void fast_digi::update_timing_parameters(const EDEPHit& hit,
                                const sand::geoinfo::stt_info::wire& wire,
                                const vec_4d& closest_point_hit,
                                const vec_4d& closest_point_wire,
                                double& wire_time,
                                double& drift_time,
                                double& signal_time,
                                double& t_hit) {

        const auto& gi = get<geoinfo>();
        const auto* stt = dynamic_cast<const sand::geoinfo::stt_info*>(&gi.tracker());
        double hit_smallest_time = stt->get_min_time(closest_point_hit, m_wire_velocity, wire);

        if (hit_smallest_time < wire_time) {
            wire_time = hit_smallest_time;
            t_hit = closest_point_hit.T();
            drift_time = closest_point_wire.T() - t_hit;
            signal_time = hit_smallest_time - closest_point_wire.T();
            
            UFW_DEBUG("    Closest point on hit: ({}, {}, {}, {})", 
                     closest_point_hit.X(), closest_point_hit.Y(), 
                     closest_point_hit.Z(), closest_point_hit.T());
            UFW_DEBUG("    Closest point on wire: ({}, {}, {}, {})", 
                     closest_point_wire.X(), closest_point_wire.Y(), 
                     closest_point_wire.Z(), closest_point_wire.T());
        }
    }

  tracker::digi::signal fast_digi::create_signal(double wire_time, double edep_total, 
                                      const channel_id& channel) {
        tracker::digi::signal signal;
        std::normal_distribution<double> gaussian_error(0.0, m_sigma_tdc);
        auto ran = gaussian_error(random_engine());
        signal.tdc = wire_time + ran;
        signal.adc = edep_total;
        signal.channel = channel;

        UFW_DEBUG("  Created signal: Channel(subdetector {}, channel {}), TDC = {}, ADC = {}",
                static_cast<int>(signal.channel.subdetector),
                static_cast<int>(signal.channel.channel),
                signal.tdc,
                signal.adc);

        return signal;
    }
  
  std::optional<tracker::digi::signal> fast_digi::process_hits_for_wire(
        const std::vector<EDEPHit>& hits, 
        const sand::geoinfo::stt_info::wire& wire,
        const geo_id& tube_id) {
        
        const auto& gi = get<geoinfo>();
        const auto* stt = dynamic_cast<const sand::geoinfo::stt_info*>(&gi.tracker());
        double wire_time = std::numeric_limits<double>::max();
        double drift_time = std::numeric_limits<double>::max();
        double signal_time = std::numeric_limits<double>::max();
        double t_hit = std::numeric_limits<double>::max();
        double edep_total = 0.0;

        for (const auto& hit : hits) {
            log_hit_debug(hit);
            
            auto closest_points = stt->closest_points(vec_4d(hit.GetStart().X(), hit.GetStart().Y(), hit.GetStart().Z(), hit.GetStart().T()), 
                                                      vec_4d(hit.GetStop().X(), hit.GetStop().Y(), hit.GetStop().Z(), hit.GetStop().T()), 
                                                      m_drift_velocity, wire);
            if (closest_points.empty()) continue;

            const vec_4d& closest_point_hit = closest_points[0];
            const vec_4d& closest_point_wire = closest_points[1];

            update_timing_parameters(hit, wire, closest_point_hit, closest_point_wire, 
                                   wire_time, drift_time, signal_time, t_hit);
            edep_total += hit.GetEnergyDeposit();
        }

        if (wire_time == std::numeric_limits<double>::max()) {
            return std::nullopt;
        }

        return create_signal(wire_time, edep_total, wire.channel);
    }

  }



