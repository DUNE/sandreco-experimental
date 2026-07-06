#include <edep_reader/edep_reader.hpp>
#include <geoinfo/drift_info.hpp>
#include <geoinfo/generic_drift_info.hpp>
#include <geoinfo/geoinfo.hpp>
#include <geoinfo/tracker_info.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>
#include <tracker/digi.h>
#include <common/sand.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <unordered_set>

namespace sand::test {

  /**
   * \class sand::test::test_drift_digi
   *
   * \brief Validation test for the drift-chamber fast digitization
   *        (\ref sand::drift::drift_fast_digi).
   *
   * Takes the \c digi collection as a requirement and produces nothing: it reads
   * the signals written by the drift digitization and cross-checks them against
   * the simulated truth.
   *
   * The drift digitization splits a hit that crosses a cell boundary into one
   * sub-hit per wire, sharing the deposited energy proportionally to the segment
   * length while keeping the *original* hit id on every sub-hit. Each fired wire
   * then becomes a single signal whose ADC is the sum of the (possibly partial)
   * segment energies on that wire, and whose truth link carries the original hit
   * ids. As a consequence a hit straddling N cells produces N signals that all
   * reference the same hit id and whose ADCs sum back to the hit's full energy.
   *
   * The checks are designed around that behaviour:
   * - every contributing hit lies within the maximum drift radius of the wire;
   * - the signal time is causal and bounded: the TDC never precedes the earliest
   *   energy deposit (beyond the TDC smearing) and never lags it by more than the
   *   maximum drift-plus-propagation time;
   * - a signal's ADC never exceeds the full energy of the hits it references (it
   *   may be smaller, because a hit's energy can be shared with neighbouring wires);
   * - globally, the total ADC equals the summed energy of the *unique* set of hits
   *   referenced by the signals. Hits the digitization discards (invalid plane,
   *   view-spanning, no wire found) are never referenced, so they drop out of both
   *   sides; split hits are counted once on both sides.
   * - The TDC window is computed per signal based on the wire's maximum drift radius
   *   and length, the configured drift and wire velocities, and the TDC resolution as
   *   a guard margin. The time window is a physically motivated expectation for when
   *   a signal should arrive, and while the digitization may produce signals outside it
   *   due to unmodeled effects or bugs, such signals are suspicious and so are flagged
   *   as errors.
   *
   * Any inconsistency is reported with \c UFW_ERROR so that the continuous
   * integration flags a regression.
   *
   * \subsection Configuration
   * | Parameter Name   | Type     | Unit  | Required/Default | Description                                    |
   * |------------------|----------|-------|------------------|------------------------------------------------|
   * | `drift_velocity` | `double` | mm/ns | Required         | Drift velocity, used for the time window.      |
   * | `wire_velocity`  | `double` | mm/ns | Required         | In-wire signal speed, used for the window.     |
   * | `sigma_tdc`      | `double` | ns    | Required         | TDC resolution; sets the causal-window margin. |
   *
   * \subsection Dependencies
   * | Type                | Comment                                     |
   * |---------------------|---------------------------------------------|
   * | `sand::geoinfo`     | Geometry                                    |
   * | `sand::edep_reader` | Simulated energy deposits of the event      |
   *
   * \subsection Requirements
   * |  Name  | Type                  | Comment                           |
   * |--------|-----------------------|-----------------------------------|
   * | `digi` | `sand::tracker::digi` | The digitized signals to validate |
   *
   * \subsection Products
   * None; this process only validates and produces no data.
   */

  class test_drift_digi : public ufw::process {
   public:
    test_drift_digi();
    void configure(const ufw::config& cfg) override;
    void run() override;
    void analyze_drift_digi();
    std::unordered_map<int, const EDEPHit*> get_hit_id_map_in_drift();
    double get_time_range(const sand::geoinfo::tracker_info::wire& wire);
    void check_truth_matching(const std::unordered_map<int, const EDEPHit*>& all_drift_hits);

   private:
    double m_drift_velocity;
    double m_wire_velocity;
    double m_sigma_tdc;
  };

  /**
   * @brief Reads and stores the configuration parameters.
   * @param cfg The JSON configuration fragment for this process.
   */
  void test_drift_digi::configure(const ufw::config& cfg) {
    UFW_DEBUG("test_drift_digi configured at: {}", fmt::ptr(this));
    m_drift_velocity = cfg.at("drift_velocity");
    m_wire_velocity  = cfg.at("wire_velocity");
    m_sigma_tdc      = cfg.at("sigma_tdc");
  }

  /// @brief Declares the process I/O: \c digi as the only requirement and no products.
  test_drift_digi::test_drift_digi() : process({{"digi", "sand::tracker::digi"}}, {}) {
    UFW_INFO("Creating a test_drift_digi process at {}", fmt::ptr(this));
  }

  /// @brief Runs the validation for the current context (event).
  void test_drift_digi::run() {
    UFW_DEBUG("test_drift_digi run called with context_id: {}", ufw::context::current()->id());
    analyze_drift_digi();
  }

  /// @brief Analyze the tracker digi signals and check their truth matching with the EDEPHit information.
  void test_drift_digi::analyze_drift_digi() {
    const auto& digi  = get<sand::tracker::digi>("digi");
    const auto& gi    = get<geoinfo>();
    const sand::geoinfo::tracker_info* drift = nullptr;

    if (const auto* d = dynamic_cast<const sand::geoinfo::drift_info*>(&gi.tracker()))
        drift = d;
    else if (const auto* gd = dynamic_cast<const sand::geoinfo::generic_drift_info*>(&gi.tracker()))
        drift = gd;
    else { UFW_ERROR("geo_info is not drift_info neither generic_drift_info"); return; }

    const auto nSignals = digi.signals.size();

    UFW_DEBUG("============== DIGI ANALYSIS ==============");
    UFW_DEBUG("Total signals: {}", nSignals);

    if (nSignals == 0) {
      UFW_DEBUG("No tracker signals found for this spill.");
      return;
    }

    const auto all_drift_hits = get_hit_id_map_in_drift();
    check_truth_matching(all_drift_hits);

    UFW_DEBUG("============== END OF DIGI ANALYSIS ==============");
  }

  /// @brief Get a map of all drift hits indexed by their IDs
  /// @return Map of drift hit IDs to pointers to the corresponding EDEPHit objects
  std::unordered_map<int, const EDEPHit*> test_drift_digi::get_hit_id_map_in_drift() {
    const auto& tree = get<sand::edep_reader>();
    std::unordered_map<int, const EDEPHit*> all_drift_hits;
    for (const auto& trj : tree) {
      const auto& hit_map = trj.GetHitMap();
      auto it             = hit_map.find(sand::subdetector_t::DRIFT);
      if (it == hit_map.end())
        continue;
      for (const auto& hit : it->second)
        all_drift_hits[hit.GetId()] = &hit;
    }
    return all_drift_hits;
  }

  /// @brief Get the maximum signal-formation time relative to the hit, used as the late edge of the causal TDC window.
  /// @param wire The wire whose drift radius and length define the time window.
  /// @return Maximum time range for signal formation on the wire, considering drift time, wire propagation time, and
  /// TDC resolution.
  double test_drift_digi::get_time_range(const sand::geoinfo::tracker_info::wire& wire) {
    double max_drift_time = wire.max_radius / m_drift_velocity;
    double max_wire_time  = wire.length() / m_wire_velocity;
    return max_drift_time + max_wire_time + 5 * m_sigma_tdc;
  }

  /// @brief Cross-check each signal against the simulated truth.
  ///
  /// Per signal: every referenced hit must be within the wire's maximum drift radius; the signal
  /// TDC must fall inside the expected time window (no earlier than the first energy deposit up to
  /// the TDC smearing, no later than the maximum drift-plus-propagation time); and the ADC must
  /// not exceed the full energy of the referenced hits (it may be smaller when a hit's energy is
  /// shared with neighbouring wires). Globally: the total ADC must equal the summed energy of the
  /// unique set of hits referenced by all signals (energy conservation through the
  /// split-and-digitize chain).
  /// @param all_drift_hits Map of every drift hit id to its EDEPHit, used to resolve the true-hit indices carried by
  /// each signal.
  void test_drift_digi::check_truth_matching(const std::unordered_map<int, const EDEPHit*>& all_drift_hits) {
    const auto& digi  = get<sand::tracker::digi>("digi");
    const auto& gi    = get<geoinfo>();
    const sand::geoinfo::tracker_info* drift = nullptr;

    if (const auto* d = dynamic_cast<const sand::geoinfo::drift_info*>(&gi.tracker()))
        drift = d;
    else if (const auto* gd = dynamic_cast<const sand::geoinfo::generic_drift_info*>(&gi.tracker()))
        drift = gd;
    else { UFW_ERROR("geo_info is not drift_info neither generic_drift_info"); return; }

    double total_adc = 0.0;                     // sum of every signal ADC
    std::unordered_set<int> referenced_hit_ids; // distinct hit ids touched by any signal

    for (const auto& signal : digi.signals) {
      UFW_DEBUG("Signal: Channel(subdetector {}, channel {}), TDC = {}, ADC = {}, true hits = {}",
                static_cast<int>(signal.channel().subdetector), static_cast<int>(signal.channel().channel),
                signal.tdc(), signal.adc(), signal.true_hits().size());

      const auto channel_wire = drift->wire_at(signal.channel());
      UFW_DEBUG("Wire: Head = {}, Tail = {}, HV = {}, Max Radius = {}", channel_wire.head, channel_wire.tail,
                channel_wire.hv, channel_wire.max_radius);

      double referenced_energy = 0.0;
      double smallest_time     = std::numeric_limits<double>::max();

      for (const auto& true_hit : signal.true_hits()) {
        UFW_DEBUG("  True hit index: {}", true_hit.get());
        auto it = all_drift_hits.find(true_hit.get());
        if (it == all_drift_hits.end()) {
          UFW_ERROR("No matching EDEPHit found for true hit index {}", true_hit.get());
          continue;
        }
        const EDEPHit* hit = it->second;
        UFW_DEBUG("EDEPHit ID {}: Energy = {}, Start = {}, Stop = {}", hit->GetId(), hit->GetEnergyDeposit(),
                  vec_4d(hit->GetStart()), vec_4d(hit->GetStop()));

        const double dist =
            channel_wire.closest_approach_segment_distance(pos_3d(hit->GetStart()), pos_3d(hit->GetStop()));
        UFW_DEBUG("Distance from wire: {}", dist);
        if (dist > channel_wire.max_radius)
          UFW_ERROR("Hit is outside the maximum drift radius");

        referenced_energy += hit->GetEnergyDeposit();
        smallest_time = std::min(smallest_time, hit->GetStart().T());
        referenced_hit_ids.insert(true_hit.get());
      }

      // The ADC is the sum of the per-wire segment energies, so it can be
      // smaller than the full energy of the referenced hits when a hit is shared across
      // neighbouring wires. It must never exceed it.
      if (signal.adc() > referenced_energy + 1e-7)
        UFW_ERROR("ADC {} exceeds the full energy of the referenced hits {}", signal.adc(), referenced_energy);
      else
        UFW_DEBUG("ADC is within the referenced hit energy");

      // Check time window: the signal cannot arrive before the earliest energy deposit (up to the
      // TDC smearing), and cannot lag it by more than the maximum drift-plus-propagation time. The
      // lower edge is the physically meaningful one; the upper edge keeps the late-arrival guard.
      const double tdc_lower = smallest_time - 5 * m_sigma_tdc;
      const double tdc_upper = smallest_time + get_time_range(channel_wire);
      if (signal.tdc() < tdc_lower || signal.tdc() > tdc_upper)
        UFW_ERROR("TDC {} is outside the expected window [{}, {}]", signal.tdc(), tdc_lower, tdc_upper);
      else
        UFW_DEBUG("TDC is within the expected window");

      total_adc += signal.adc();
    }

    // Global energy conservation. A hit split across several wires yields several signals that
    // all carry the same hit id and whose ADCs sum back to the hit's full energy; counting each
    // referenced hit's full energy once therefore must equal the total ADC. Hits the digitization
    // discarded are never referenced and so are excluded from both sides.
    double referenced_total_energy = 0.0;
    for (int id : referenced_hit_ids)
      referenced_total_energy += all_drift_hits.at(id)->GetEnergyDeposit();

    // Diagnostics: full event energy and how much was dropped before digitization.
    double all_hits_energy = 0.0;
    for (const auto& [id, hit] : all_drift_hits)
      all_hits_energy += hit->GetEnergyDeposit();

    UFW_DEBUG("Energy totals: all hits = {}, referenced = {}, discarded = {}, total ADC = {}", all_hits_energy,
              referenced_total_energy, all_hits_energy - referenced_total_energy, total_adc);

    const double global_tol = 1e-7 + 1e-9 * std::abs(referenced_total_energy);
    if (std::abs(referenced_total_energy - total_adc) > global_tol)
      UFW_ERROR("Global energy mismatch: referenced hits sum = {}, total ADC = {}", referenced_total_energy, total_adc);
    else
      UFW_DEBUG("Global energy is conserved (referenced hits sum matches total ADC)");
  }
} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::test_drift_digi)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::test_drift_digi)