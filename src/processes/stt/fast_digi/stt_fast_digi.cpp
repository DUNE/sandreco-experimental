/**
 * \file
 * \brief Implementation of the \ref sand::stt::stt_fast_digi process.
 *
 * Contains the STT fast-digitization algorithm: grouping of the energy
 * deposits per tube, hit-to-wire closest-approach geometry, drift and in-wire
 * propagation timing, and construction of the digitized signals. The public
 * interface and parameters are documented in stt_fast_digi.hpp.
 */

#include <stt_fast_digi.hpp>

#include <edep_reader/edep_reader.hpp>
#include <geoinfo/drift_info.hpp>
#include <geoinfo/geoinfo.hpp>
#include <geoinfo/stt_info.hpp>
#include <geoinfo/tracker_info.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>
#include <tracker/digi.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <algorithm>
#include <limits>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>

namespace sand::stt {

  /**
   * \class sand::stt::stt_fast_digi
   *
   * \brief Fast digitization of STT straw-tube energy deposits into wire signals.
   *
   * Groups the STRAW energy deposits of the event per straw tube and turns the hits
   * collected on each fired tube into one or more digis. The hit arrival
   * times at the readout end (drift plus in-wire propagation) are sorted; a digi opens at the
   * first arrival and collects every hit arriving within `integration_time`; hits arriving in
   * the following `dead_time` are discarded and the next arrival opens a new digi on the same
   * wire. Every hit collected in a digi becomes one of its signals, storing the raw arrival
   * time, the deposited energy and the hit id for Monte Carlo truth matching. The digi TDC is
   * the earliest signal time Gaussian-smeared by `sigma_tdc`; the digi ADC is the sum of the
   * signal energies.
   *
   * \subsection Configuration
   * | Parameter Name   | Type     | Unit  | Required/Default | Description                          |
   * |------------------|----------|-------|------------------|--------------------------------------|
   * | `drift_velocity` | `double` | mm/ns | Required         | Drift velocity of ionized particles. |
   * | `wire_velocity`  | `double` | mm/ns | Required         | Signal transit speed along the wire. |
   * | `sigma_tdc`      | `double` | ns    | Required         | Gaussian time resolution of the TDC. |
   * | `integration_time` | `double` | ns  | Required         | Signal integration window.           |
   * | `dead_time`      | `double` | ns    | Required         | Dead time after the integration window. |
   *
   * \subsection Dependencies
   * | Type                     | Comment                                |
   * |--------------------------|----------------------------------------|
   * | `sand::geoinfo`          | Geometry                        |
   * | `sand::edep_reader`      | Simulated energy deposits of the event |
   * | `sand::root_tgeomanager` | ROOT geometry navigator                |
   *
   * \subsection Requirements
   * None; the inputs are unique objects fetched by type (see Dependencies).
   *
   * \subsection Products
   * |  Name  | Type                  | Comment                |
   * |--------|-----------------------|------------------------|
   * | `digi` | `sand::tracker::digi_container` | One digi per integration window per fired tube, one signal per contributing hit |
   */
  /**
   * \brief Reads and stores the configuration parameters.
   * \param cfg The JSON configuration fragment for this process.
   */

  void stt_fast_digi::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_drift_velocity = cfg.at("drift_velocity");
    m_wire_velocity  = cfg.at("wire_velocity");
    m_sigma_tdc        = cfg.at("sigma_tdc");
    m_integration_time = cfg.at("integration_time");
    m_dead_time        = cfg.at("dead_time");
  }

  /// \brief Declares the process I/O: no requirements and a single product \c digi.
  stt_fast_digi::stt_fast_digi() : process({}, {{"digi", "sand::tracker::digi_container"}}) {
    UFW_DEBUG("Creating a stt_fast_digi process at {}", fmt::ptr(this));
  }

  /**
   * \brief Digitizes the current event.
   *
   * Groups the STRAW hits per tube and fills the \c digi product with one
   * digi per integration window on each fired tube.
   */
  void stt_fast_digi::run() {
    UFW_DEBUG("Running stt_fast_digi process at {}", fmt::ptr(this));
    UFW_DEBUG(" STT subdetector implementation");
    // Two phases: first assign every STRAW hit to its straw tube, then turn the
    // hits collected on each fired tube into one digi per integration window.
    std::map<geo_id, std::vector<EDEPHit>> hits_by_tube = group_hits_by_tube();
    digitize_hits_in_tubes(hits_by_tube);
  }

  /**
   * \brief Groups the STRAW hits of the current event by the tube they belong to.
   *
   * The midpoint of each hit is located in the geometry through the navigator
   * and mapped to the corresponding tube identifier.
   *
   * \return A map from tube geometry id (\c geo_id) to the hits collected in that tube.
   */
  std::map<geo_id, std::vector<EDEPHit>> stt_fast_digi::group_hits_by_tube() {
    const auto& gi   = get<geoinfo>();
    const auto& tree = get<sand::edep_reader>();
    auto& tgm        = ufw::context::current()->instance<root_tgeomanager>();
    std::map<geo_id, std::vector<EDEPHit>> hits_by_tube;
    const auto* stt = dynamic_cast<const sand::geoinfo::stt_info*>(&gi.tracker());

    for (const auto& trj : tree) {
      const auto& hit_map = trj.GetHitMap(); // pointer, not value
      if (hit_map.find(sand::subdetector_t::STT) == hit_map.end())
        continue;

      UFW_DEBUG("Found {} STRAW hits for trajectory with ID {}", hit_map.at(sand::subdetector_t::STT).size(), trj.GetId());

      for (const auto& hit : hit_map.at(sand::subdetector_t::STT)) {
        // Use the hit midpoint to locate the geometry node, then derive the
        // tube identifier (geo_id) from the navigator path.
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

  /**
   * \brief Builds the digis of every fired tube and appends them to the \c digi product.
   * \param hits_by_tube The hits of the event grouped per tube.
   */
  void stt_fast_digi::digitize_hits_in_tubes(const std::map<geo_id, std::vector<EDEPHit>>& hits_by_tube) {
    const auto& gi  = get<geoinfo>();
    auto& digi_out  = set<sand::tracker::digi_container>("digi");
    const auto* stt = dynamic_cast<const sand::geoinfo::stt_info*>(&gi.tracker());
    std::size_t nsignals = 0;

    if (!stt)
      return;

    for (const auto& [tube_id, hits] : hits_by_tube) {
      const auto* wire = stt->get_wire_by_id(tube_id);

      if (!wire) {
        UFW_WARN("No wire found for geo_id {}", tube_id);
        continue;
      }

      UFW_DEBUG("Digitizing hits for tube {}", tube_id);

      auto tube_digis = process_hits_for_wire(hits, *wire);
      UFW_DEBUG(" Number of digis in tube: {}", tube_digis.size());
      for (auto& d : tube_digis) {
        nsignals += d.signals.size();
        digi_out.digis.emplace_back(std::move(d));
      }
    }
    UFW_INFO("Digitization complete. Created {} digis with {} signals in total.", digi_out.digis.size(), nsignals);
  }

  /**
   * \brief Applies the TDC resolution to a time.
   * \param time The true time [ns].
   * \return \p time smeared with a Gaussian of width \c sigma_tdc [ns].
   */
  double stt_fast_digi::smear_time(double time) {
    std::normal_distribution<double> gaussian_error(0.0, m_sigma_tdc); // FIXME should be member
    return time + gaussian_error(random_engine());
  }

  /**
   * \brief Computes the digis of a single tube from the hits assigned to its wire.
   *
   * For each hit the points of closest approach between the hit segment and the
   * wire are found and the arrival time at the readout end (hit time + drift time
   * + in-wire propagation time) is evaluated. Arrivals are sorted in time and
   * grouped into digis: a digi opens at the first arrival not yet used and
   * collects all arrivals within \c integration_time of it; the arrivals in the
   * following \c dead_time are discarded; the next arrival opens a new digi.
   * Each collected arrival becomes a signal holding the raw time, energy and hit
   * id. The digi TDC is the earliest signal time smeared by \c sigma_tdc, the
   * digi ADC the sum of the signal energies.
   *
   * \param hits The hits collected on the tube.
   * \param wire The geometry of the wire.
   * \return The time-ordered digis of the tube; empty if it has no hits.
   */
  tracker::digi_container::digi_collection
  stt_fast_digi::process_hits_for_wire(const std::vector<EDEPHit>& hits, const sand::geoinfo::stt_info::wire& wire) {
    struct arrival {
      double time;
      double edep;
      std::decay_t<decltype(std::declval<const EDEPHit&>().GetId())> hit_id;
    };

    std::vector<arrival> arrivals;
    arrivals.reserve(hits.size());
    for (const auto& hit : hits) {
      auto closest_points = closest_points_hit_wire(
          vec_4d(hit.GetStart().X(), hit.GetStart().Y(), hit.GetStart().Z(), hit.GetStart().T()),
          vec_4d(hit.GetStop().X(), hit.GetStop().Y(), hit.GetStop().Z(), hit.GetStop().T()), m_drift_velocity, wire);

      const vec_4d& closest_point_hit  = closest_points.first;
      const vec_4d& closest_point_wire = closest_points.second;

      // closest_point_wire.T() already includes the drift time; add the in-wire propagation.
      const double arrival_time = get_min_time(closest_point_wire, m_wire_velocity, wire);
      UFW_DEBUG("  Hit {}: closest point on hit {}, on wire {}, arrival time {}", hit.GetId(), closest_point_hit,
                closest_point_wire, arrival_time);
      arrivals.push_back({arrival_time, hit.GetEnergyDeposit(), hit.GetId()});
    }

    std::stable_sort(arrivals.begin(), arrivals.end(),
                     [](const arrival& a, const arrival& b) { return a.time < b.time; });

    tracker::digi_container::digi_collection wire_digis;
    std::size_t i = 0;
    while (i < arrivals.size()) {
      const double window_start = arrivals[i].time;
      const double window_end   = window_start + m_integration_time;
      const double dead_end     = window_end + m_dead_time;

      // Signals: raw arrival time and deposited energy of every hit in [window_start, window_end).
      auto& current      = wire_digis.emplace_back();
      double digi_energy = 0.0;
      double digi_time   = std::numeric_limits<double>::max();
      for (; i < arrivals.size() && arrivals[i].time < window_end; ++i) {
        tracker::digi_container::digi::signal signal(wire.daq_channel, arrivals[i].time, arrivals[i].edep);
        signal.insert(arrivals[i].hit_id);
        digi_energy += signal.energy();
        digi_time = std::min(digi_time, signal.t());
        current.signals.push_back(std::move(signal));
      }

      // Digi: TDC = earliest signal time (smeared by sigma_tdc), ADC = sum of the signal energies.
      current.set_tdc(smear_time(digi_time));
      current.set_adc(digi_energy);
      UFW_DEBUG("  Digi in [{}, {}): TDC = {}, ADC = {}, {} signals.", window_start, window_end, current.tdc(),
                current.adc(), current.signals.size());

      // Dead time: discard every arrival in [window_end, dead_end).
      for (; i < arrivals.size() && arrivals[i].time < dead_end; ++i)
        UFW_DEBUG("  Hit {} arrives at {} during dead time [{}, {}), discarded.", arrivals[i].hit_id,
                  arrivals[i].time, window_end, dead_end);
    }

    return wire_digis;
  }

  /**
   * \brief Finds the points of closest approach between a hit segment and a wire.
   *
   * Each returned point also carries the time at which it is reached: the hit
   * point keeps the interpolated hit time, the wire point adds the drift time
   * obtained from \p v_drift.
   *
   * \param hit_start Start point of the hit segment (space-time).
   * \param hit_stop  Stop point of the hit segment (space-time).
   * \param v_drift   Drift velocity used to set the time of the wire point [mm/ns].
   * \param w         The wire geometry.
   * \return A pair {closest point on the hit, closest point on the wire}.
   */
  std::pair<vec_4d, vec_4d> stt_fast_digi::closest_points_hit_wire(const vec_4d& hit_start,
                                                                   const vec_4d& hit_stop, // TO-DO move to fast_digi
                                                                   double v_drift,
                                                                   const geoinfo::tracker_info::wire& w) const {
    std::pair<vec_4d, vec_4d> closest_points;

    pos_3d start(hit_start.X(), hit_start.Y(), hit_start.Z());
    pos_3d stop(hit_stop.X(), hit_stop.Y(), hit_stop.Z());

    auto seg_params = w.closest_approach_segment(start, stop);

    std::vector<vec_4d> result;

    double& t       = seg_params.first;  // Parameter along s
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

  /**
   * \brief Earliest time a signal generated at \p point reaches the wire readout end.
   * \param point           Space-time point on the wire.
   * \param v_signal_inwire In-wire signal propagation speed [mm/ns].
   * \param w               The wire geometry; its \c head is the readout end.
   * \return The arrival time at the readout end [ns].
   */
  double stt_fast_digi::get_min_time(const vec_4d& point, double v_signal_inwire,
                                     const geoinfo::tracker_info::wire& w) const {
    return point.T() + sqrt((pos_3d(point.Vect()) - w.head).Mag2()) / v_signal_inwire;
  }

} // namespace sand::stt

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::stt::stt_fast_digi)
