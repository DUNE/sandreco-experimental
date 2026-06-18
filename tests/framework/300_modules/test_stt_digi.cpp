#include <common/version.h>
#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <edep_reader/edep_reader.hpp>
#include <geoinfo/drift_info.hpp>
#include <geoinfo/geoinfo.hpp>
#include <geoinfo/stt_info.hpp>
#include <geoinfo/tracker_info.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>
#include <tracker/digi.h>


namespace sand::test {

  class test_stt_digi : public ufw::process {
  	public:
			test_stt_digi();
			void configure(const ufw::config& cfg) override;
			void run() override;
			void analyze_stt_digi();
			std::unordered_map<int, const EDEPHit*> get_hit_id_map_in_straws();
			void check_truth_matching(const std::unordered_map<int, const EDEPHit*>& all_straw_hits);

  	private:
  };

  void test_stt_digi::configure(const ufw::config& cfg) {
    UFW_DEBUG("test_stt_digi configured at: {}", fmt::ptr(this));
  }

  test_stt_digi::test_stt_digi() : process({{"digi", "sand::tracker::digi"}}, {}) {
    UFW_INFO("Creating a test_stt_digi process at {}", fmt::ptr(this));
  }

  void test_stt_digi::run() {
    UFW_DEBUG("test_stt_digi run called with context_id: {}", ufw::context::current()->id());
    analyze_stt_digi();
  }

  void test_stt_digi::analyze_stt_digi() {
    const auto& digi = get<sand::tracker::digi>("digi");
    const auto& gi   = get<geoinfo>();
    const auto* stt  = dynamic_cast<const sand::geoinfo::stt_info*>(&gi.tracker());

    if (!stt) {
      UFW_ERROR("Failed to cast tracker to stt_info");
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

  /// @brief Check that the true hits associated with each tracker signal match the expected energy deposit 
	///				 and are within the maximum drift radius of the corresponding wire.
  /// @param all_straw_hits 
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
      }

      if (std::abs(total_energy_deposit - signal.adc()) > 1e-3)
        UFW_ERROR("Energy mismatch: true hits sum = {}, ADC = {}", total_energy_deposit, signal.adc());
      else
        UFW_DEBUG("Energy deposit matches ADC");
    }
  }
} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::test_stt_digi)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::test_stt_digi)
