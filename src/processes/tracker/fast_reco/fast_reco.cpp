#include "fast_reco.hpp"

#include <caf/caf_wrapper.hpp>
#include <truth_filler/g4_id_map.hpp>

#include <ufw/factory.hpp>

#include <duneanaobj/StandardRecord/SRTrack.h>
#include <duneanaobj/StandardRecord/SRVector3D.h>

#include <TDatabasePDG.h>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>
#include <vector>

namespace sand::tracker {

  namespace {
    ::caf::SRVector3D normalize_to_direction(float px, float py, float pz) {
      float const mag = std::sqrt(px * px + py * py + pz * pz);
      return (mag > 0.f) ? ::caf::SRVector3D{px / mag, py / mag, pz / mag} : ::caf::SRVector3D{0.f, 0.f, 0.f};
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

    /// @return the last two points in time (last, previous); nullptr where there are not enough points.
    ///
    /// Scans the per-subdetector points in place instead of flattening and sorting them
    /// (EDEPTrajectory::GetTrajectoryPointsVect copies every point). On equal times the later point in
    /// iteration order wins, as in GetTrajectoryPointsVect's stable sort.
    std::pair<EDEPTrajectoryPoint const*, EDEPTrajectoryPoint const*>
    last_two_points(EDEPTrajectoryPoints const& points) {
      EDEPTrajectoryPoint const* last{};
      EDEPTrajectoryPoint const* prev{};
      for (auto const& [det, det_points] : points) {
        for (auto const& point : det_points) {
          auto const t = point.GetPosition().T();
          if (last == nullptr || t >= last->GetPosition().T()) {
            prev = last;
            last = &point;
          } else if (prev == nullptr || t >= prev->GetPosition().T()) {
            prev = &point;
          }
        }
      }
      return {last, prev};
    }

    /// @return the direction at the last trajectory point in time, `start_dir` if there are fewer than two points.
    ::caf::SRVector3D end_direction(EDEPTrajectoryPoints const& points, ::caf::SRVector3D const& start_dir) {
      double constexpr stopped_momentum{1e-6};

      auto const [last_point, prev_point] = last_two_points(points);
      if (prev_point == nullptr) {
        return start_dir;
      }

      auto const& last_mom = last_point->GetMomentum();
      if (std::hypot(last_mom.x(), last_mom.y(), last_mom.z()) > stopped_momentum) {
        return normalize_to_direction(static_cast<float>(last_mom.x()), static_cast<float>(last_mom.y()),
                                      static_cast<float>(last_mom.z()));
      }

      auto const& prev = prev_point->GetPosition();
      auto const& last = last_point->GetPosition();
      return normalize_to_direction(static_cast<float>(last.x() - prev.x()), static_cast<float>(last.y() - prev.y()),
                                    static_cast<float>(last.z() - prev.z()));
    }

    bool is_track_like(int pdg) {
      int const abs_pdg = std::abs(pdg);
      return abs_pdg == 13 || abs_pdg == 211 || abs_pdg == 321 || abs_pdg == 2212;
    }

    bool is_shower_like(int pdg) {
      int const abs_pdg = std::abs(pdg);
      return abs_pdg == 11 || abs_pdg == 22 || abs_pdg == 111;
    }

    /// edep-sim positions are in mm, CAF positions in cm.
    ::caf::SRVector3D position_cm(sand::vec_4d const& v) {
      return {static_cast<float>(v.X() * 0.1), static_cast<float>(v.Y() * 0.1), static_cast<float>(v.Z() * 0.1)};
    }
  } // namespace

  std::vector<EDEPHit> tracker_hits(EDEPHitsMap const& hit_map) {
    std::vector<EDEPHit> hits;
    for (auto const det : {sand::subdetector_t::STT, sand::subdetector_t::DRIFT}) {
      if (auto const it = hit_map.find(det); it != hit_map.end()) {
        hits.insert(hits.end(), it->second.begin(), it->second.end());
      }
    }
    std::stable_sort(hits.begin(), hits.end(),
                     [](EDEPHit const& a, EDEPHit const& b) { return a.GetStart().T() < b.GetStart().T(); });
    return hits;
  }

  double total_energy_deposit(std::vector<EDEPHit> const& hits) {
    return std::accumulate(hits.begin(), hits.end(), 0.,
                           [](double sum, EDEPHit const& hit) { return sum + hit.GetEnergyDeposit(); });
  }

  ::caf::SRTrack track_from_hits(std::vector<EDEPHit> const& hits, EDEPTrajectoryPoints const& points,
                                 ::caf::SRTrueParticle const& true_part, ::caf::TrueParticleID const& id) {
    UFW_ASSERT(!hits.empty(), "track_from_hits: a track needs at least one tracker hit");

    auto const& first = hits.front().GetStart();
    auto const& last  = hits.back().GetStop();

    ::caf::SRTrack track{};

    // from the tracker hits
    track.start = position_cm(first);
    track.end   = position_cm(last);
    track.time  = first.T();
    track.len_cm =
        static_cast<float>(std::accumulate(hits.begin(), hits.end(), 0.,
                                           [](double sum, EDEPHit const& h) { return sum + h.GetTrackLength(); })
                           * 0.1);
    track.Evis = static_cast<float>(total_energy_deposit(hits)); // MeV

    // still from the truth (directions and energy at tracker entry: next step)
    track.dir    = normalize_to_direction(true_part.p.px, true_part.p.py, true_part.p.pz);
    track.enddir = end_direction(points, track.dir);
    track.E      = true_part.p.E * 1000.; // SRTrack::E is in MeV, SRTrueParticle::p in GeV
    track.charge = charge_from_pdg(true_part.pdg);
    track.qual   = 1.f;

    track.truth.push_back(id);
    track.truthOverlap.push_back(1.f);

    return track;
  }

  ::caf::SRShower shower_from_hits(std::vector<EDEPHit> const& hits, ::caf::SRTrueParticle const& true_part,
                                   ::caf::TrueParticleID const& id) {
    UFW_ASSERT(!hits.empty(), "shower_from_hits: a shower needs at least one tracker hit");

    auto const& first = hits.front().GetStart();

    ::caf::SRShower shower{};

    // from the tracker hits
    shower.start = position_cm(first);
    shower.time  = first.T();
    shower.Evis  = static_cast<float>(total_energy_deposit(hits)); // MeV

    // still from the truth (direction at tracker entry: next step)
    shower.direction = normalize_to_direction(true_part.p.px, true_part.p.py, true_part.p.pz);

    shower.truth.push_back(id);
    shower.truthOverlap.push_back(1.f);

    return shower;
  }

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
    auto const& truth_branch = get<sand::caf::truth_branch_wrapper>("in_truth");
    auto& tracker_reco       = set<sand::caf::tracker_reco_wrapper>("out_tracker");

    auto const g4_ids = sand::common::g4_id_maps(edep.GetChildrenTrajectories());

    int n_no_hits{}, n_below_threshold{}, n_unclassified{};
    for (std::size_t ixn{}, nu_size{truth_branch.nu.size()}; ixn != nu_size; ++ixn) {
      auto const& ids = g4_ids.at(ixn);

      auto add_all = [&](std::vector<::caf::SRTrueParticle> const& particles, ::caf::TrueParticleID::PartType type) {
        for (std::size_t idx{}, particles_size{particles.size()}; idx != particles_size; ++idx) {
          auto const& p  = particles[idx];
          auto const trj = edep.GetTrajectory(ids.to_spill_id(p.G4ID));
          if (trj == edep.end()) {
            UFW_ERROR("nu[{}]: true particle G4ID {} (spill id {}) has no edep-sim trajectory", ixn, p.G4ID,
                      ids.to_spill_id(p.G4ID));
          }
          auto const hits = tracker_hits(trj->GetHitMap());
          if (hits.empty()) {
            ++n_no_hits;
            continue;
          }
          if (total_energy_deposit(hits) < m_min_energy_deposit) {
            ++n_below_threshold;
            continue;
          }

          ::caf::TrueParticleID const id{static_cast<int>(ixn), type, static_cast<int>(idx)};
          if (is_track_like(p.pdg)) {
            tracker_reco.tracks.push_back(track_from_hits(hits, trj->GetTrajectoryPoints(), p, id));
            ++tracker_reco.ntracks;
          } else if (is_shower_like(p.pdg)) {
            tracker_reco.showers.push_back(shower_from_hits(hits, p, id));
            ++tracker_reco.nshowers;
          } else {
            ++n_unclassified;
          }
        }
      };
      add_all(truth_branch.nu[ixn].prim, ::caf::TrueParticleID::kPrimary);
      add_all(truth_branch.nu[ixn].sec, ::caf::TrueParticleID::kSecondary);
    }
    UFW_DEBUG("tracker::fast_reco: {} tracks, {} showers; {} particles without tracker hits, {} below "
              "min_energy_deposit ({} MeV), {} above it but neither track- nor shower-like",
              tracker_reco.ntracks, tracker_reco.nshowers, n_no_hits, n_below_threshold, m_min_energy_deposit,
              n_unclassified);
  }

} // namespace sand::tracker

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::tracker::fast_reco);
