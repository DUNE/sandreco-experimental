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

  void test_stt_digi::analyze_stt_digi()
    {
        const auto& tree = get<sand::edep_reader>();
        const auto& digi = get<sand::tracker::digi>("digi");
        const auto& gi   = get<geoinfo>();
        const auto* stt = dynamic_cast<const sand::geoinfo::stt_info*>(&gi.tracker());

        const auto nSignals  = digi.signals.size();

        UFW_DEBUG("============== DIGI ANALYSIS ==============");
        UFW_DEBUG("Total signals : {}", nSignals);

        if (nSignals == 0) {
            UFW_WARN("No signals found in this event!");
            return;
        }

        std::vector<const EDEPHit*> all_straw_hits;
        for (const auto& trj : tree) {
            const auto& hit_map = trj.GetHitMap();
            auto it = hit_map.find(component::STRAW);
            if (it == hit_map.end())
                continue;
            for (const auto& hit : it->second)
                all_straw_hits.push_back(&hit);
        }

        for (const auto& signal : digi.signals) {
            UFW_DEBUG("Signal: Channel(subdetector {}, channel {}), TDC = {}, ADC = {}, number of true hits = {}",
                    static_cast<int>(signal.channel().subdetector), static_cast<int>(signal.channel().channel),
                    signal.tdc(), signal.adc(), signal.true_hits().size());

            auto channel_wire = stt->wire_at(signal.channel());
            UFW_DEBUG("    Wire info: Head = {}, Tail = {}, HV = {}, Max Radius = {}",
                            channel_wire.head, channel_wire.tail, channel_wire.hv, channel_wire.max_radius);

            double total_energy_deposit = 0.0;
            for (const auto& true_hit : signal.true_hits()) {
                UFW_DEBUG("  True hit index: {}", true_hit.get());
                auto it = std::find_if(all_straw_hits.begin(), all_straw_hits.end(),
                    [&true_hit](const EDEPHit* hit) { return hit->GetId() == true_hit.get(); });
                if (it == all_straw_hits.end()) {
                    UFW_ERROR("    No matching EDEPHit found for true hit index {}", true_hit.get());
                } else {
                    UFW_DEBUG("    Found matching EDEPHit with ID {}: Energy Deposit = {}, Start Position = {}, Stop Position = {}",
                                (*it)->GetId(), (*it)->GetEnergyDeposit(), vec_4d((*it)->GetStart()), vec_4d((*it)->GetStop()));
                    double distance_from_wire = channel_wire.closest_approach_segment_distance(pos_3d((*it)->GetStart()), pos_3d((*it)->GetStop()));
                    UFW_DEBUG("    Distance from wire: {}", distance_from_wire);
                    if(distance_from_wire > channel_wire.max_radius) {
                        UFW_ERROR("    Hit is outside the maximum drift radius of the wire");
                    }
                    total_energy_deposit += (it != all_straw_hits.end()) ? (*it)->GetEnergyDeposit() : 0.0;
                }
            
            }
            time_mean = signal.true_hits().empty() ? 0.0 : time_mean / signal.true_hits().size();
            if (abs(total_energy_deposit - signal.adc()) > 1e-3) {
                UFW_ERROR("  Total energy deposit from true hits ({}) does not match signal ADC ({})", total_energy_deposit, signal.adc());
            } else {
                UFW_DEBUG("  Total energy deposit from true hits matches signal ADC");
            }

            UFW_DEBUG("===============================================");
        }
    }
} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::test_stt_digi)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::test_stt_digi)
