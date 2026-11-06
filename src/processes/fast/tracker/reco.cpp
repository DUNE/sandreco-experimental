#include "fast_reco.hpp"
#include "common_utils.hpp"

#include <caf/caf_wrapper.hpp>
#include <edep_reader/edep_reader.hpp>
#include <spill_maps/spill_maps.hpp>

#include <ufw/factory.hpp>

#include <duneanaobj/StandardRecord/SRTrueParticle.h>

namespace sand::tracker {

  fast_reco::fast_reco()
    : process{{{"in_truth", "sand::caf::truth_branch_wrapper"}}, {{"out_tracker", "sand::caf::tracker_reco_wrapper"}}} {
  }

  void fast_reco::configure(ufw::config const& cfg) {
    process::configure(cfg);
    m_min_energy_deposit = cfg.value("min_energy_deposit", 0.);

    UFW_ASSERT(m_min_energy_deposit >= 0., "tracker::fast_reco: min_energy_deposit must be >= 0, got {}",
               m_min_energy_deposit);
  }

  void fast_reco::run() {
    auto const& edep         = instance<sand::edep_reader>();
    auto const& g4_ids       = instance<sand::spill_maps>();
    auto const& truth_branch = get<sand::caf::truth_branch_wrapper>("in_truth");
    auto& tracker_reco       = set<sand::caf::tracker_reco_wrapper>("out_tracker");

    int n_no_hits{}, n_below_threshold{}, n_unclassified{}, n_no_tracker_points{};
    sand::fast_reco::for_each_true_particle(
        truth_branch, edep, g4_ids,
        [&](::caf::SRTrueParticle const& p, ::caf::TrueParticleID const& id, EDEPTrajectory const& trj) {
          auto const volume_hits =
              sand::fast_reco::volume_hits_through(trj, {subdetector_t::STT, subdetector_t::DRIFT});
          if (volume_hits.hits.empty()) {
            ++n_no_hits;
            return;
          }
          if (sand::fast_reco::energy_deposit(volume_hits) < m_min_energy_deposit) {
            ++n_below_threshold;
            return;
          }

          if (sand::fast_reco::is_track_like(p.pdg)) {
            n_no_tracker_points += volume_hits.entry ? 0 : 1;
            tracker_reco.tracks.push_back(sand::fast_reco::track_from(volume_hits, p, id));
            ++tracker_reco.ntracks;
          } else if (sand::fast_reco::is_shower_like(p.pdg)) {
            n_no_tracker_points += volume_hits.entry ? 0 : 1;
            tracker_reco.showers.push_back(sand::fast_reco::shower_from(volume_hits, id));
            ++tracker_reco.nshowers;
          } else {
            ++n_unclassified;
          }
        });
    UFW_DEBUG("tracker::fast_reco: {} tracks, {} showers; {} particles without tracker hits, {} below "
              "min_energy_deposit ({} MeV), {} above it but neither track- nor shower-like; {} objects without "
              "trajectory points in the tracker (directions from the hits, track E unknown)",
              tracker_reco.ntracks, tracker_reco.nshowers, n_no_hits, n_below_threshold, m_min_energy_deposit,
              n_unclassified, n_no_tracker_points);
  }

} // namespace sand::tracker

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::tracker::fast_reco);
