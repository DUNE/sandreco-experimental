#include "fast_reco.hpp"

#include <caf/caf_wrapper.hpp>
#include <truth_filler/g4_id_map.hpp>

#include <ufw/factory.hpp>

#include <TDatabasePDG.h>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace sand::tracker {

  namespace {
    ::caf::SRVector3D normalize_to_direction(float px, float py, float pz) {
      float const mag = std::sqrt(px * px + py * py + pz * pz);
      return (mag > 0.f) ? ::caf::SRVector3D{px / mag, py / mag, pz / mag} : ::caf::SRVector3D{0.f, 0.f, 0.f};
    }

    float distance(::caf::SRVector3D const& a, ::caf::SRVector3D const& b) {
      float const dx = b.x - a.x;
      float const dy = b.y - a.y;
      float const dz = b.z - a.z;
      return std::sqrt(dx * dx + dy * dy + dz * dz);
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

    ::caf::SRVector3D end_direction_from_edep(EDEPTrajectory const* trj, ::caf::SRVector3D const& start_dir) {
      double constexpr stopped_momentum{1e-6};

      if (trj == nullptr) {
        return start_dir;
      }

      auto const points = trj->GetTrajectoryPointsVect();
      if (points.size() < 2) {
        return start_dir;
      }

      auto const& last_mom = points.back().GetMomentum();
      if (std::hypot(last_mom.x(), last_mom.y(), last_mom.z()) > stopped_momentum) {
        return normalize_to_direction(static_cast<float>(last_mom.x()), static_cast<float>(last_mom.y()),
                                      static_cast<float>(last_mom.z()));
      }

      auto const& prev = points[points.size() - 2].GetPosition();
      auto const& last = points.back().GetPosition();
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

  ::caf::SRTrack track_from_true(::caf::SRTrueParticle const& true_part, ::caf::TrueParticleID const& id,
                                 EDEPTrajectory const* trj) {
    ::caf::SRTrack track{};

    track.start  = true_part.start_pos;
    track.end    = true_part.end_pos;
    track.dir    = normalize_to_direction(true_part.p.px, true_part.p.py, true_part.p.pz);
    track.enddir = end_direction_from_edep(trj, track.dir);
    track.time   = true_part.time;
    track.E      = true_part.p.E * 1000.; // SRTrack::E is in MeV, SRTrueParticle::p in GeV
    track.Evis   = track.E;
    track.len_cm = distance(track.start, track.end);
    track.charge = charge_from_pdg(true_part.pdg);
    track.qual   = 1.f;

    track.truth.push_back(id);
    track.truthOverlap.push_back(1.f);

    return track;
  }

  ::caf::SRShower shower_from_true(::caf::SRTrueParticle const& true_part, ::caf::TrueParticleID const& id) {
    ::caf::SRShower shower{};

    shower.start     = true_part.start_pos;
    shower.direction = normalize_to_direction(true_part.p.px, true_part.p.py, true_part.p.pz);
    shower.Evis      = true_part.p.E * 1000.; // MeV, like SRTrack::E (SRTrueParticle::p is in GeV)

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

    for (std::size_t ixn{}, nu_size{truth_branch.nu.size()}; ixn != nu_size; ++ixn) {
      auto const& ids = g4_ids.at(ixn);

      auto add_all = [&](std::vector<::caf::SRTrueParticle> const& particles, ::caf::TrueParticleID::PartType type) {
        for (std::size_t idx{}, particles_size{particles.size()}; idx != particles_size; ++idx) {
          auto const& p  = particles[idx];
          auto const trj = edep.GetTrajectory(ids.to_spill_id(p.G4ID));
          if (trj == edep.end() || !has_tracker_hits(*trj) || total_energy_deposit(*trj) < m_min_energy_deposit) {
            continue;
          }

          ::caf::TrueParticleID const id{static_cast<int>(ixn), type, static_cast<int>(idx)};
          if (is_track_like(p.pdg)) {
            tracker_reco.tracks.push_back(track_from_true(p, id, &*trj));
            ++tracker_reco.ntracks;
          } else if (is_shower_like(p.pdg)) {
            tracker_reco.showers.push_back(shower_from_true(p, id));
            ++tracker_reco.nshowers;
          }
        }
      };
      add_all(truth_branch.nu[ixn].prim, ::caf::TrueParticleID::kPrimary);
      add_all(truth_branch.nu[ixn].sec, ::caf::TrueParticleID::kSecondary);
    }
  }

} // namespace sand::tracker

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::tracker::fast_reco);
