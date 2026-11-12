#include "reco.hpp"

#include <caf/caf_wrapper.hpp>
#include <edep_reader/edep_reader.hpp>
#include <spill_maps/spill_maps.hpp>

#include <ufw/factory.hpp>
#include <ufw/utils.hpp>

#include <TDatabasePDG.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <numeric>
#include <vector>

namespace sand::fast::tracker {

  namespace {

    /// The tracker volumes: straws and drift chambers
    constexpr std::array tracker_volumes{subdetector_t::STT, subdetector_t::DRIFT};

    /// Below this momentum [MeV] a particle is stopped and its momentum has no direction
    constexpr double stopped_momentum{1e-6};

    /// The hits of `trj` in the tracker, ordered by start time. STT and DRIFT are alternative designs, so at most one
    /// of them has hits, and the edep_reader already orders the hits of each component.
    std::vector<EDEPHit> tracker_hits(EDEPTrajectory const& trj) {
      for (auto const volume : tracker_volumes) {
        if (auto const it = trj.GetHitMap().find(volume); it != trj.GetHitMap().end()) {
          return it->second;
        }
      }
      return {};
    }

    /// The earliest trajectory point of `trj` in the tracker, where the tracker would measure the momentum; null if
    /// edep-sim stored none there
    EDEPTrajectoryPoint const* entry_point(EDEPTrajectory const& trj) {
      EDEPTrajectoryPoint const* entry{};
      for (auto const volume : tracker_volumes) {
        if (auto const it = trj.GetTrajectoryPoints().find(volume); it != trj.GetTrajectoryPoints().end()) {
          for (auto const& point : it->second) {
            if (entry == nullptr || point.GetPosition().T() < entry->GetPosition().T()) {
              entry = &point;
            }
          }
        }
      }
      return entry;
    }

    ::caf::SRVector3D normalize_to_direction(double x, double y, double z) {
      auto const fx   = static_cast<float>(x);
      auto const fy   = static_cast<float>(y);
      auto const fz   = static_cast<float>(z);
      float const mag = std::sqrt(fx * fx + fy * fy + fz * fz);
      return (mag > 0.f) ? ::caf::SRVector3D{fx / mag, fy / mag, fz / mag} : ::caf::SRVector3D{0.f, 0.f, 0.f};
    }

    /// Direction of a hit, from its start to its stop
    ::caf::SRVector3D direction_of(EDEPHit const& hit) {
      return normalize_to_direction(hit.GetStop().X() - hit.GetStart().X(), hit.GetStop().Y() - hit.GetStart().Y(),
                                    hit.GetStop().Z() - hit.GetStart().Z());
    }

    /// Direction at the tracker entry: the momentum at the entry point, else the first hit
    ::caf::SRVector3D entry_direction(EDEPTrajectoryPoint const* entry, std::vector<EDEPHit> const& hits) {
      if (entry != nullptr && std::sqrt(entry->GetMomentum().Mag2()) > stopped_momentum) {
        auto const& p = entry->GetMomentum();
        return normalize_to_direction(p.X(), p.Y(), p.Z());
      }
      return direction_of(hits.front());
    }

    /// edep-sim positions are in mm, CAF positions in cm
    ::caf::SRVector3D position_cm(sand::vec_4d const& v) {
      return {static_cast<float>(v.X() * 0.1), static_cast<float>(v.Y() * 0.1), static_cast<float>(v.Z() * 0.1)};
    }

    double energy_deposit(std::vector<EDEPHit> const& hits) {
      return std::accumulate(hits.begin(), hits.end(), 0.,
                             [](double sum, EDEPHit const& hit) { return sum + hit.GetEnergyDeposit(); });
    }

    double track_length_mm(std::vector<EDEPHit> const& hits) {
      return std::accumulate(hits.begin(), hits.end(), 0.,
                             [](double sum, EDEPHit const& hit) { return sum + hit.GetTrackLength(); });
    }

    /// Mass hypothesis from the true pdg (perfect PID) [MeV]
    double mass_mev(int pdg) {
      auto const* p = TDatabasePDG::Instance()->GetParticle(pdg);
      return p != nullptr ? p->Mass() * 1000. : 0.; // TDatabasePDG masses are in GeV
    }

    short charge_from_pdg(int pdg) {
      if (auto const* p = TDatabasePDG::Instance()->GetParticle(pdg)) {
        double const q = p->Charge() / 3.0;
        if (q > 0.5) {
          return 1;
        }
        if (q < -0.5) {
          return -1;
        }
      }
      return 0;
    }

    /// True for the long-lived charged particles that leave a track: mu±, pi±, K±, p and pbar
    bool is_track_like(int pdg) {
      int const abs_pdg = std::abs(pdg);
      return abs_pdg == 13 || abs_pdg == 211 || abs_pdg == 321 || abs_pdg == 2212;
    }

    /// True for the electromagnetic particles that start a shower: e±, gamma, pi0
    bool is_shower_like(int pdg) {
      int const abs_pdg = std::abs(pdg);
      return abs_pdg == 11 || abs_pdg == 22 || abs_pdg == 111;
    }

    /// Position, time, length and Evis from the hits; momentum (dir, E) at the entry point, E NaN without one;
    /// enddir from the last hit
    ::caf::SRTrack track_from(std::vector<EDEPHit> const& hits, EDEPTrajectoryPoint const* entry,
                              ::caf::SRTrueParticle const& true_part, ::caf::TrueParticleID const& id, double b_field) {
      ::caf::SRTrack track{};

      track.start  = position_cm(hits.front().GetStart());
      track.end    = position_cm(hits.back().GetStop());
      track.time   = hits.front().GetStart().T();
      track.len_cm = static_cast<float>(track_length_mm(hits) * 0.1);
      track.Evis   = static_cast<float>(energy_deposit(hits)); // MeV

      track.dir    = entry_direction(entry, hits);
      track.enddir = direction_of(hits.back());
      if (entry != nullptr) {
        auto const& p  = entry->GetMomentum(); // MeV
        auto const m   = mass_mev(true_part.pdg);
        track.E        = static_cast<float>(std::sqrt(p.Mag2() + m * m)); // MeV
        auto const p_T = std::hypot(p.Y(), p.Z());                        // transverse to the field, along x
        track.R        = (p_T * 1e-3) / (0.3 * b_field) * 100.;           // MeV -> GeV, m -> cm
      }

      track.charge = charge_from_pdg(true_part.pdg);
      track.qual   = 1.f;

      track.truth.push_back(id);
      track.truthOverlap.push_back(1.f);
      return track;
    }

    /// Start, time and Evis from the hits; direction at the entry point
    ::caf::SRShower shower_from(std::vector<EDEPHit> const& hits, EDEPTrajectoryPoint const* entry,
                                ::caf::TrueParticleID const& id) {
      ::caf::SRShower shower{};

      shower.start     = position_cm(hits.front().GetStart());
      shower.time      = hits.front().GetStart().T();
      shower.Evis      = static_cast<float>(energy_deposit(hits)); // MeV
      shower.direction = entry_direction(entry, hits);

      shower.truth.push_back(id);
      shower.truthOverlap.push_back(1.f);
      return shower;
    }

  } // namespace

  reco::reco()
    : process{{{"in_truth", "sand::caf::truth_branch_wrapper"}}, {{"out_tracker", "sand::caf::tracker_reco_wrapper"}}} {
  }

  void reco::configure(ufw::config const& cfg) {
    process::configure(cfg);
    m_min_energy_deposit = cfg.value("min_energy_deposit", 0.);
    m_b_field            = cfg.value("b_field_magnitude", 0.6);

    UFW_ASSERT(m_min_energy_deposit >= 0., "fast::tracker::reco: min_energy_deposit must be >= 0, got {}",
               m_min_energy_deposit);
    UFW_ASSERT(m_b_field > 0., "fast::tracker::reco: b_field must be > 0., got {}", m_b_field);
  }

  void reco::run() {
    auto const& edep    = instance<sand::edep_reader>();
    auto const& id_maps = instance<sand::spill_maps>();
    auto const& truth   = get<sand::caf::truth_branch_wrapper>("in_truth");
    auto& tracker       = set<sand::caf::tracker_reco_wrapper>("out_tracker");

    int n_no_hits{}, n_below_threshold{};

    for (std::size_t ixn{}; ixn != truth.nu.size(); ++ixn) {
      auto const& ids = id_maps.at(ixn);

      // Every true particle of the interaction: primaries, then secondaries
      for (auto const type : {::caf::TrueParticleID::kPrimary, ::caf::TrueParticleID::kSecondary}) {
        auto const& particles = (type == ::caf::TrueParticleID::kPrimary) ? truth.nu[ixn].prim : truth.nu[ixn].sec;
        for (std::size_t idx{}; idx != particles.size(); ++idx) {
          auto const& p = particles[idx];
          ::caf::TrueParticleID const id{static_cast<int>(ixn), type, static_cast<int>(idx)};

          auto const trj = edep.GetTrajectory(ids.to_spill_id(p.G4ID));
          if (trj == edep.end()) {
            UFW_ERROR("nu[{}]: true particle G4ID {} has no edep-sim trajectory", ixn, p.G4ID);
          }

          // Detectability: hits in the tracker, with enough deposited energy
          auto const hits = tracker_hits(*trj);
          if (hits.empty()) {
            ++n_no_hits;
            continue;
          }
          if (energy_deposit(hits) < m_min_energy_deposit) {
            ++n_below_threshold;
            continue;
          }

          if (is_track_like(p.pdg)) {
            tracker.tracks.push_back(track_from(hits, entry_point(*trj), p, id, m_b_field));
          } else if (is_shower_like(p.pdg)) {
            tracker.showers.push_back(shower_from(hits, entry_point(*trj), id));
          }
        }
      }
    }
    tracker.ntracks  = tracker.tracks.size();
    tracker.nshowers = tracker.showers.size();

    UFW_DEBUG("fast::tracker::reco: {} tracks, {} showers; {} particles without tracker hits, {} below "
              "min_energy_deposit ({} MeV)",
              tracker.ntracks, tracker.nshowers, n_no_hits, n_below_threshold, m_min_energy_deposit);
  }

} // namespace sand::fast::tracker

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::fast::tracker::reco);
