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

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <unordered_set>
#include <vector>

namespace sand::test {

  /**
   * \class sand::test::test_drift_digi
   *
   * \brief Validation test for the drift-chamber fast digitization
   *        (\ref sand::drift::drift_fast_digi).
   *
   * Takes the \c digi container as a requirement and produces nothing: it reads
   * the digis written by the drift digitization and cross-checks them against
   * the simulated truth.
   *
   * The drift digitization splits a hit that crosses a cell boundary into one
   * sub-hit per wire, sharing the deposited energy proportionally to the segment
   * length while keeping the *original* hit id on every sub-hit. As a consequence a
   * hit straddling N cells is referenced by up to N signals on different wires, each
   * carrying only part of its energy.
   *
   * The checks are designed around that behaviour:
   * - every digi has signals, all on one channel, all within `integration_time` of
   *   the earliest one; its ADC equals the sum of its signal energies and its TDC
   *   matches the earliest signal time within the TDC smearing;
   * - consecutive digis on the same channel are at least `integration_time + dead_time`
   *   apart;
   * - every signal references exactly one hit, which lies within the maximum drift
   *   radius of the wire; the signal energy never exceeds the hit energy (it may be
   *   smaller when the hit is split); the signal time never precedes the hit and never
   *   lags it by more than the maximum drift-plus-propagation time;
   * - globally, the summed signal energy equals the summed energy of the *unique* set
   *   of referenced hits. Hits the digitization discards (invalid plane, view-spanning,
   *   no wire found) are never referenced, so they drop out of both sides. With a
   *   non-zero dead time one segment of a split hit can be discarded while another is
   *   kept, so only the upper bound is required then.
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
   * | `integration_time` | `double` | ns  | Required         | Integration window of the digitization.        |
   * | `dead_time`      | `double` | ns    | Required         | Dead time of the digitization.                 |
   *
   * \subsection Dependencies
   * | Type                | Comment                                     |
   * |---------------------|---------------------------------------------|
   * | `sand::geoinfo`     | Geometry                                    |
   * | `sand::edep_reader` | Simulated energy deposits of the event      |
   *
   * \subsection Requirements
   * |  Name  | Type                            | Comment                        |
   * |--------|---------------------------------|--------------------------------|
   * | `digi` | `sand::tracker::digi_container` | The digitized digis to validate |
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
    void check_digit_structure();
    void check_truth_matching(const std::unordered_map<int, const EDEPHit*>& all_drift_hits);

   private:
    double m_drift_velocity;
    double m_wire_velocity;
    double m_sigma_tdc;
    double m_integration_time;
    double m_dead_time;
  };

  /**
   * @brief Reads and stores the configuration parameters.
   * @param cfg The JSON configuration fragment for this process.
   */
  void test_drift_digi::configure(const ufw::config& cfg) {
    UFW_DEBUG("test_drift_digi configured at: {}", fmt::ptr(this));
    m_drift_velocity = cfg.at("drift_velocity");
    m_wire_velocity  = cfg.at("wire_velocity");
    m_sigma_tdc        = cfg.at("sigma_tdc");
    m_integration_time = cfg.at("integration_time");
    m_dead_time        = cfg.at("dead_time");
  }

  /// @brief Declares the process I/O: \c digi as the only requirement and no products.
  test_drift_digi::test_drift_digi() : process({{"digi", "sand::tracker::digi_container"}}, {}) {
    UFW_INFO("Creating a test_drift_digi process at {}", fmt::ptr(this));
  }

  /// @brief Runs the validation for the current context (event).
  void test_drift_digi::run() {
    UFW_DEBUG("test_drift_digi run called with context_id: {}", ufw::context::current()->id());
    analyze_drift_digi();
  }

  /// @brief Analyze the tracker digi signals and check their truth matching with the EDEPHit information.
  void test_drift_digi::analyze_drift_digi() {
    const auto& digi  = get<sand::tracker::digi_container>("digi");
    const auto& gi    = get<geoinfo>();

    if (!dynamic_cast<const sand::geoinfo::drift_info*>(&gi.tracker())
        && !dynamic_cast<const sand::geoinfo::generic_drift_info*>(&gi.tracker())) {
      UFW_ERROR("geo_info is not drift_info neither generic_drift_info");
      return;
    }

    std::size_t nSignals = 0;
    for (const auto& wire_digi : digi.digis)
      nSignals += wire_digi.signals.size();

    UFW_DEBUG("============== DIGI ANALYSIS ==============");
    UFW_DEBUG("Total digis: {}, total signals: {}", digi.digis.size(), nSignals);

    if (nSignals == 0) {
      UFW_DEBUG("No tracker signals found for this spill.");
      return;
    }

    check_digit_structure();
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

  /// @brief Check the internal structure of every digi and the separation of digis on the same channel.
  ///
  /// Per digi: it has signals, all on one channel, all within `integration_time` of the earliest one; its ADC
  /// equals the sum of the signal energies and its TDC is the earliest signal time up to the TDC smearing.
  /// Per channel: consecutive digis start at least `integration_time + dead_time` apart.
  void test_drift_digi::check_digit_structure() {
    const auto& digi        = get<sand::tracker::digi_container>("digi");
    const double tdc_tolerance = 5 * m_sigma_tdc;
    std::map<uint64_t, std::vector<double>> window_starts_by_channel;

    for (const auto& d : digi.digis) {
      if (d.signals.empty()) {
        UFW_ERROR("Found a digi with no signals");
        continue;
      }
      const auto channel = d.signals.front().channel();
      double earliest    = std::numeric_limits<double>::max();
      double energy_sum  = 0.0;
      for (const auto& signal : d.signals) {
        if (signal.channel() != channel)
          UFW_ERROR("Signals of the same digi on different channels: {} vs {}", static_cast<int>(channel.channel),
                    static_cast<int>(signal.channel().channel));
        earliest = std::min(earliest, signal.t());
        energy_sum += signal.energy();
      }
      for (const auto& signal : d.signals)
        if (signal.t() >= earliest + m_integration_time)
          UFW_ERROR("Signal at {} is outside the integration window [{}, {})", signal.t(), earliest,
                    earliest + m_integration_time);

      if (std::abs(d.adc() - energy_sum) > 1e-7)
        UFW_ERROR("Digi ADC {} differs from the sum of its signal energies {}", d.adc(), energy_sum);
      if (std::abs(d.tdc() - earliest) > tdc_tolerance)
        UFW_ERROR("Digi TDC {} differs from the earliest signal time {} by more than {}", d.tdc(), earliest,
                  tdc_tolerance);

      window_starts_by_channel[channel.raw].push_back(earliest);
    }

    for (auto& [channel, starts] : window_starts_by_channel) {
      std::sort(starts.begin(), starts.end());
      for (std::size_t i = 1; i < starts.size(); ++i)
        if (starts[i] - starts[i - 1] < m_integration_time + m_dead_time)
          UFW_ERROR("Digis on the same channel start {} ns apart, less than integration + dead time {} ns",
                    starts[i] - starts[i - 1], m_integration_time + m_dead_time);
    }
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
  /// Per signal: exactly one referenced hit, within the wire's maximum drift radius; the signal
  /// energy must not exceed the hit energy (it may be smaller when the hit is split across
  /// wires); the raw signal time must not precede the hit nor lag it by more than the maximum
  /// drift-plus-propagation time. Globally: the summed signal energy must equal the summed
  /// energy of the unique set of referenced hits (energy conservation through the
  /// split-and-digitize chain).
  /// @param all_drift_hits Map of every drift hit id to its EDEPHit, used to resolve the true-hit indices carried by
  /// each signal.
  void test_drift_digi::check_truth_matching(const std::unordered_map<int, const EDEPHit*>& all_drift_hits) {
    const auto& digi  = get<sand::tracker::digi_container>("digi");
    const auto& gi    = get<geoinfo>();
    const sand::geoinfo::tracker_info* drift = nullptr;

    if (const auto* d = dynamic_cast<const sand::geoinfo::drift_info*>(&gi.tracker()))
        drift = d;
    else if (const auto* gd = dynamic_cast<const sand::geoinfo::generic_drift_info*>(&gi.tracker()))
        drift = gd;
    else { UFW_ERROR("geo_info is not drift_info neither generic_drift_info"); return; }

    double total_adc = 0.0;                     // sum of every signal ADC
    std::unordered_set<int> referenced_hit_ids; // distinct hit ids touched by any signal

    for (const auto& d : digi.digis) {
      UFW_DEBUG("Digi: TDC = {}, ADC = {}, signals = {}", d.tdc(), d.adc(), d.signals.size());

      for (const auto& signal : d.signals) {
        UFW_DEBUG("Signal: Channel(subdetector {}, channel {}), time = {}, energy = {}, true hits = {}",
                  static_cast<int>(signal.channel().subdetector), static_cast<int>(signal.channel().channel),
                  signal.t(), signal.energy(), signal.true_hits().size());
        total_adc += signal.energy();

        if (signal.true_hits().size() != 1) {
          UFW_ERROR("Signal references {} hits, expected exactly one", signal.true_hits().size());
          continue;
        }

        const auto& true_hit = *signal.true_hits().begin();
        auto it              = all_drift_hits.find(true_hit.get());
        if (it == all_drift_hits.end()) {
          UFW_ERROR("No matching EDEPHit found for true hit index {}", true_hit.get());
          continue;
        }
        const EDEPHit* hit = it->second;
        referenced_hit_ids.insert(true_hit.get());
        UFW_DEBUG("EDEPHit ID {}: Energy = {}, Start = {}, Stop = {}", hit->GetId(), hit->GetEnergyDeposit(),
                  vec_4d(hit->GetStart()), vec_4d(hit->GetStop()));

        const auto channel_wire = drift->wire_at(signal.channel());
        const double dist =
            channel_wire.closest_approach_segment_distance(pos_3d(hit->GetStart()), pos_3d(hit->GetStop()));
        UFW_DEBUG("Distance from wire: {}", dist);
        if (dist > channel_wire.max_radius)
          UFW_ERROR("Hit is outside the maximum drift radius");

        // A split hit leaves only part of its energy on each wire, so the signal energy can be smaller than
        // the hit energy but never larger.
        if (signal.energy() > hit->GetEnergyDeposit() + 1e-7)
          UFW_ERROR("Signal energy {} exceeds the referenced hit energy {}", signal.energy(), hit->GetEnergyDeposit());

        // The signal time is the raw arrival time: it cannot precede the hit and cannot lag it by more than the
        // maximum drift-plus-propagation time.
        const double earliest_hit_time = std::min(hit->GetStart().T(), hit->GetStop().T());
        const double latest_allowed    = earliest_hit_time + get_time_range(channel_wire);
        if (signal.t() < earliest_hit_time || signal.t() > latest_allowed)
          UFW_ERROR("Signal time {} is outside the expected window [{}, {}]", signal.t(), earliest_hit_time,
                    latest_allowed);
      }
    }

    // Global energy conservation. A hit split across several wires yields several signals that
    // all carry the same hit id and whose energies sum back to the hit's full energy; counting each
    // referenced hit's full energy once therefore must equal the summed signal energy. Hits the digitization
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

    // With a dead time, a segment of a split hit can be discarded on one wire while another
    // segment is kept on a neighbouring wire: the hit is then referenced but only part of its
    // energy reaches the ADC, so only the upper bound holds.
    const double global_tol = 1e-7 + 1e-9 * std::abs(referenced_total_energy);
    if (m_dead_time == 0.0 && std::abs(referenced_total_energy - total_adc) > global_tol)
      UFW_ERROR("Global energy mismatch: referenced hits sum = {}, total ADC = {}", referenced_total_energy, total_adc);
    else if (total_adc > referenced_total_energy + global_tol)
      UFW_ERROR("Total ADC {} exceeds the referenced hits sum {}", total_adc, referenced_total_energy);
    else
      UFW_DEBUG("Global energy is consistent (referenced hits sum {} vs total ADC {})", referenced_total_energy,
                total_adc);
  }
} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::test_drift_digi)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::test_drift_digi)