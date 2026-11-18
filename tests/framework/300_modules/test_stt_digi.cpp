#include <edep_reader/edep_reader.hpp>
#include <geoinfo/drift_info.hpp>
#include <geoinfo/geoinfo.hpp>
#include <geoinfo/stt_info.hpp>
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
#include <vector>

namespace sand::test {

  /**
   * \class sand::test::test_stt_digi
   *
   * \brief Validation test for the STT fast digitization (\ref sand::stt::stt_fast_digi).
   *
   * Unlike a digitization process, this module takes the \c digi collection as a
   * requirement and produces nothing: it reads the digis written by the STT
   * digitization and cross-checks them against the simulated truth. Each digi is
   * one integration window on one tube; each of its signals is one contributing hit
   * (raw arrival time and deposited energy). It verifies that
   * - every digi has signals, all on one channel, all within `integration_time` of
   *   the earliest one;
   * - the digi ADC equals the sum of its signal energies and the digi TDC matches the
   *   earliest signal time within the TDC smearing;
   * - consecutive digis on the same channel are at least `integration_time + dead_time`
   *   apart;
   * - every signal references exactly one hit, whose deposited energy equals the
   *   signal energy (STT hits are never split), that lies within the maximum drift
   *   radius of the wire, and whose time is compatible with the signal time.
   *
   * Any inconsistency is reported with \c UFW_ERROR so that the continuous
   * integration flags a regression.
   *
   * \subsection Configuration
   * | Parameter Name   | Type     | Unit  | Required/Default | Description                                |
   * |------------------|----------|-------|------------------|--------------------------------------------|
   * | `drift_velocity` | `double` | mm/ns | Required         | Drift velocity, used for the time window.  |
   * | `wire_velocity`  | `double` | mm/ns | Required         | In-wire signal speed, used for the window. |
   * | `sigma_tdc`      | `double` | ns    | Required         | TDC resolution, used for the window.       |
   * | `integration_time` | `double` | ns  | Required         | Integration window of the digitization.    |
   * | `dead_time`      | `double` | ns    | Required         | Dead time of the digitization.             |
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
   * | `digi` | `sand::tracker::digi_container` | The digis to validate |
   *
   * \subsection Products
   * None; this process only validates and produces no data.
   */

  class test_stt_digi : public ufw::process {
   public:
    test_stt_digi();
    void configure(const ufw::config& cfg) override;
    void run() override;
    void analyze_stt_digi();
    std::unordered_map<int, const EDEPHit*> get_hit_id_map_in_straws();
    double get_time_range(const sand::geoinfo::tracker_info::wire& wire);
    void check_digi_structure();
    void check_truth_matching(const std::unordered_map<int, const EDEPHit*>& all_straw_hits);

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
  void test_stt_digi::configure(const ufw::config& cfg) {
    UFW_DEBUG("test_stt_digi configured at: {}", fmt::ptr(this));
    m_drift_velocity = cfg.at("drift_velocity");
    m_wire_velocity  = cfg.at("wire_velocity");
    m_sigma_tdc        = cfg.at("sigma_tdc");
    m_integration_time = cfg.at("integration_time");
    m_dead_time        = cfg.at("dead_time");
  }

  /// @brief Declares the process I/O: \c digi as the only requirement and no products.
  test_stt_digi::test_stt_digi() : process({{"digi", "sand::tracker::digi_container"}}, {}) {
    UFW_INFO("Creating a test_stt_digi process at {}", fmt::ptr(this));
  }

  /// @brief Runs the validation for the current context (event).
  void test_stt_digi::run() {
    UFW_DEBUG("test_stt_digi run called with context_id: {}", ufw::context::current()->id());
    analyze_stt_digi();
  }

  /// @brief Analyze the STT digis and check their structure and truth matching with the EDEPHit information.
  void test_stt_digi::analyze_stt_digi() {
    const auto& digi_in = get<sand::tracker::digi_container>("digi");
    const auto& gi      = get<geoinfo>();
    const auto* stt     = dynamic_cast<const sand::geoinfo::stt_info*>(&gi.tracker());

    if (!stt) {
      UFW_ERROR("The tracker info object is not STT");
      return;
    }

    std::size_t nSignals = 0;
    for (const auto& d : digi_in.digis)
      nSignals += d.signals.size();

    UFW_DEBUG("============== DIGI ANALYSIS ==============");
    UFW_DEBUG("Total digis: {}, total signals: {}", digi_in.digis.size(), nSignals);

    if (digi_in.digis.empty()) {
      UFW_DEBUG("No tracker digis found for this spill.");
      return;
    }

    check_digi_structure();
    const auto all_straw_hits = get_hit_id_map_in_straws();
    check_truth_matching(all_straw_hits);

    UFW_DEBUG("============== END OF DIGI ANALYSIS ==============");
  }

  /// @brief Get a map of all straw hits indexed by their IDs
  /// @return Map of straw hit IDs to pointers to the corresponding EDEPHit objects
  std::unordered_map<int, const EDEPHit*> test_stt_digi::get_hit_id_map_in_straws() {
    const auto& tree = get<sand::edep_reader>();
    std::unordered_map<int, const EDEPHit*> all_straw_hits;
    for (const auto& trj : tree) {
      const auto& hit_map = trj.GetHitMap();
      auto it             = hit_map.find(sand::subdetector_t::STT);
      if (it == hit_map.end())
        continue;
      for (const auto& hit : it->second)
        all_straw_hits[hit.GetId()] = &hit;
    }
    return all_straw_hits;
  }

  /// @brief Get the maximum time range for signal formation on a given wire
  /// @param wire The wire whose drift radius and length define the time window.
  /// @return Maximum time range for signal formation on the wire, considering drift time, wire propagation time, and
  /// TDC resolution.
  double test_stt_digi::get_time_range(const sand::geoinfo::tracker_info::wire& wire) {
    double max_drift_time = wire.max_radius / m_drift_velocity;
    double max_wire_time  = wire.length() / m_wire_velocity;
    return max_drift_time + max_wire_time + 5 * m_sigma_tdc;
  }

  /// @brief Check the internal structure of every digi and the separation of digis on the same channel.
  ///
  /// Per digi: it has signals, all on one channel, all within `integration_time` of the earliest one; its ADC
  /// equals the sum of the signal energies and its TDC is the earliest signal time up to the TDC smearing.
  /// Per channel: consecutive digis start at least `integration_time + dead_time` apart.
  void test_stt_digi::check_digi_structure() {
    const auto& digi_in       = get<sand::tracker::digi_container>("digi");
    const double tdc_tolerance = 5 * m_sigma_tdc;
    std::map<uint64_t, std::vector<double>> window_starts_by_channel;

    for (const auto& d : digi_in.digis) {
      if (d.signals.empty()) {
        UFW_ERROR("Found a digi with no signals");
        continue;
      }
      const auto channel = d.signals.front().channel();
      double earliest    = std::numeric_limits<double>::max();
      double energy_sum  = 0.0;
      for (const auto& signal : d.signals) {
        if (!(signal.channel() == channel))
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

  /// @brief Check every signal against the true hit it references: exactly one hit, same energy, within the
  ///        maximum drift radius of the wire, and with a time compatible with the drift-plus-propagation window.
  /// @param all_straw_hits Map of every STRAW hit id to its EDEPHit, used to resolve the true-hit indices carried by
  /// each signal.
  void test_stt_digi::check_truth_matching(const std::unordered_map<int, const EDEPHit*>& all_straw_hits) {
    const auto& digi_in = get<sand::tracker::digi_container>("digi");
    const auto& gi      = get<geoinfo>();
    const auto* stt     = dynamic_cast<const sand::geoinfo::stt_info*>(&gi.tracker());

    for (const auto& d : digi_in.digis) {
      UFW_DEBUG("Digi: TDC = {}, ADC = {}, signals = {}", d.tdc(), d.adc(), d.signals.size());

      for (const auto& signal : d.signals) {
        UFW_DEBUG("Signal: Channel(subdetector {}, channel {}), time = {}, energy = {}, true hits = {}",
                  static_cast<int>(signal.channel().subdetector), static_cast<int>(signal.channel().channel),
                  signal.t(), signal.energy(), signal.true_hits().size());

        if (signal.true_hits().size() != 1) {
          UFW_ERROR("Signal references {} hits, expected exactly one", signal.true_hits().size());
          continue;
        }

        const auto& true_hit = *signal.true_hits().begin();
        auto it              = all_straw_hits.find(true_hit.get());
        if (it == all_straw_hits.end()) {
          UFW_ERROR("No matching EDEPHit found for true hit index {}", true_hit.get());
          continue;
        }
        const EDEPHit* hit = it->second;
        UFW_DEBUG("EDEPHit ID {}: Energy = {}, Start = {}, Stop = {}", hit->GetId(), hit->GetEnergyDeposit(),
                  vec_4d(hit->GetStart()), vec_4d(hit->GetStop()));

        const auto channel_wire = stt->wire_at(signal.channel());
        const double dist =
            channel_wire.closest_approach_segment_distance(pos_3d(hit->GetStart()), pos_3d(hit->GetStop()));
        UFW_DEBUG("Distance from wire: {}", dist);
        if (dist > channel_wire.max_radius)
          UFW_ERROR("Hit is outside the maximum drift radius");

        if (std::abs(hit->GetEnergyDeposit() - signal.energy()) > 1e-7)
          UFW_ERROR("Energy mismatch: hit energy = {}, signal energy = {}", hit->GetEnergyDeposit(), signal.energy());

        // The signal time is the raw arrival time: it cannot precede the hit and cannot lag it by more than the
        // maximum drift-plus-propagation time.
        const double earliest_hit_time = std::min(hit->GetStart().T(), hit->GetStop().T());
        if (signal.t() < earliest_hit_time || signal.t() > earliest_hit_time + get_time_range(channel_wire))
          UFW_ERROR("Signal time {} is outside the expected window [{}, {}]", signal.t(), earliest_hit_time,
                    earliest_hit_time + get_time_range(channel_wire));
      }
    }
  }
} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::test_stt_digi)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::test_stt_digi)