#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>
#include <common/version.h>

#include <edep_reader/edep_reader.hpp>
#include <geoinfo/drift_info.hpp>
#include <geoinfo/geoinfo.hpp>
#include <geoinfo/stt_info.hpp>
#include <geoinfo/tracker_info.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>
#include <tracker/digi.h>

namespace sand::test {

  /**
   * \class sand::test::test_stt_digi
   *
   * \brief Validation test for the STT fast digitization (\ref sand::stt::stt_fast_digi).
   *
   * Unlike a digitization process, this module takes the \c digi collection as a
   * requirement and produces nothing: it reads the signals written by the STT
   * digitization and cross-checks them against the simulated truth. Using the
   * truth-hit indices attached to each signal it verifies that
   * - the signal ADC equals the sum of the deposited energies of its true hits;
   * - the signal time is compatible with the expected drift-plus-propagation window;
   * - every contributing hit lies within the maximum drift radius of the wire.
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

  class test_stt_digi : public ufw::process {
   public:
    test_stt_digi();
    void configure(const ufw::config& cfg) override;
    void run() override;
    void analyze_stt_digi();
    std::unordered_map<int, const EDEPHit*> get_hit_id_map_in_straws();
    double get_time_range(const sand::geoinfo::tracker_info::wire& wire);
    void check_truth_matching(const std::unordered_map<int, const EDEPHit*>& all_straw_hits);

   private:
    double m_drift_velocity;
    double m_wire_velocity;
    double m_sigma_tdc;
  };

  /**
   * @brief Reads and stores the configuration parameters.
   * @param cfg The JSON configuration fragment for this process.
   */
  void test_stt_digi::configure(const ufw::config& cfg) {
    UFW_DEBUG("test_stt_digi configured at: {}", fmt::ptr(this));
    m_drift_velocity = cfg.at("drift_velocity");
    m_wire_velocity  = cfg.at("wire_velocity");
    m_sigma_tdc      = cfg.at("sigma_tdc");
  }

  /// @brief Declares the process I/O: \c digi as the only requirement and no products.
  test_stt_digi::test_stt_digi() : process({{"digi", "sand::tracker::digi"}}, {}) {
    UFW_INFO("Creating a test_stt_digi process at {}", fmt::ptr(this));
  }

  /// @brief Runs the validation for the current context (event).
  void test_stt_digi::run() {
    UFW_DEBUG("test_stt_digi run called with context_id: {}", ufw::context::current()->id());
    analyze_stt_digi();
  }

  /// @brief Analyze the STT digi signals and check their truth matching with the EDEPHit information.
  void test_stt_digi::analyze_stt_digi() {
    const auto& digi = get<sand::tracker::digi>("digi");
    const auto& gi   = get<geoinfo>();
    const auto* stt  = dynamic_cast<const sand::geoinfo::stt_info*>(&gi.tracker());

    if (!stt) {
      UFW_ERROR("The tracker info object is not STT");
      return;
    }

    const auto nSignals = digi.signals.size();

    UFW_DEBUG("============== DIGI ANALYSIS ==============");
    UFW_DEBUG("Total signals: {}", nSignals);

    if (nSignals == 0) {
      UFW_DEBUG("No tracker signals found for this spill.");
      return;
    }

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
      auto it             = hit_map.find(component::STRAW);
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

  /// @brief Check that the true hits associated with each tracker signal match the expected energy deposit,
  ///        the time matches the maximum signal formation time range and are within the maximum drift radius of the
  ///        corresponding wire.
  /// @param all_straw_hits Map of every STRAW hit id to its EDEPHit, used to resolve the true-hit indices carried by
  /// each signal.
  void test_stt_digi::check_truth_matching(const std::unordered_map<int, const EDEPHit*>& all_straw_hits) {
    const auto& digi = get<sand::tracker::digi>("digi");
    const auto& gi   = get<geoinfo>();
    const auto* stt  = dynamic_cast<const sand::geoinfo::stt_info*>(&gi.tracker());

    for (const auto& signal : digi.signals) {
      UFW_DEBUG("Signal: Channel(subdetector {}, channel {}), TDC = {}, ADC = {}, true hits = {}",
                static_cast<int>(signal.channel().subdetector), static_cast<int>(signal.channel().channel),
                signal.tdc(), signal.adc(), signal.true_hits().size());

      const auto channel_wire = stt->wire_at(signal.channel());
      UFW_DEBUG("Wire: Head = {}, Tail = {}, HV = {}, Max Radius = {}", channel_wire.head, channel_wire.tail,
                channel_wire.hv, channel_wire.max_radius);

      double total_energy_deposit = 0.0;
      double smallest_time        = std::numeric_limits<double>::max();

      for (const auto& true_hit : signal.true_hits()) {
        UFW_DEBUG("  True hit index: {}", true_hit.get());
        auto it = all_straw_hits.find(true_hit.get());
        if (it == all_straw_hits.end()) {
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

        total_energy_deposit += hit->GetEnergyDeposit();
        smallest_time = std::min(smallest_time, hit->GetStart().T());
      }

      if (std::abs(total_energy_deposit - signal.adc()) > 1e-7)
        UFW_ERROR("Energy mismatch: true hits sum = {}, ADC = {}", total_energy_deposit, signal.adc());
      else
        UFW_DEBUG("Energy deposit matches ADC");

      if (abs(smallest_time - signal.tdc()) > get_time_range(channel_wire))
        UFW_ERROR("Smallest time is outside the expected range");
      else
        UFW_DEBUG("Smallest time is within the expected range");
    }
  }
} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::test_stt_digi)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::test_stt_digi)