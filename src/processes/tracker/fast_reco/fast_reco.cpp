#include "fast_reco.hpp"

#include <caf/caf_wrapper.hpp>
#include <truth_filler/g4_id_map.hpp>

#include <ufw/factory.hpp>

#include <TDatabasePDG.h>

#include <cmath>
#include <algorithm>

namespace sand::tracker {

  namespace {
    // "Straw" in STT geometries, "DriftVolume" in the drift ones (see sand::string_to_component)
    bool has_tracker_hits(EDEPTrajectory const& trj) {
      return trj.HasHitInDetector(sand::subdetector_t::STT) || trj.HasHitInDetector(sand::subdetector_t::DRIFT);
    }

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

  TrackerG4IDs g4ids_from_edep(sand::edep_reader const& tree) {
    TrackerG4IDs ids;
    for (auto const& trj : tree) {
      if (has_tracker_hits(trj)) {
        ids.push_back(trj.GetId());
      }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
  }

  ::caf::SRTrack track_from_true(::caf::SRTrueParticle const& true_part, ::caf::TrueParticleID const& id,
                                 EDEPTrajectory const* trj) {
    ::caf::SRTrack track{};

    track.start  = true_part.start_pos;
    track.end    = true_part.end_pos;
    track.dir    = normalize_to_direction(true_part.p.px, true_part.p.py, true_part.p.pz);
    track.enddir = end_direction_from_edep(trj, track.dir);
    track.time   = true_part.time;
    track.E      = true_part.p.E;
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
    shower.Evis      = true_part.p.E;

    shower.truth.push_back(id);
    shower.truthOverlap.push_back(1.f);

    return shower;
  }

  fast_reco::fast_reco()
    : process{{{"in_truth", "sand::caf::truth_branch_wrapper"}}, {{"out_tracker", "sand::caf::tracker_reco_wrapper"}}} {
  }

  void fast_reco::run() {
    auto const& edep         = instance<sand::edep_reader>();
    auto const& truth_branch = get<sand::caf::truth_branch_wrapper>("in_truth");
    auto& tracker_reco       = set<sand::caf::tracker_reco_wrapper>("out_tracker");

    auto const tracker_ids = g4ids_from_edep(edep);
    auto const g4_ids      = sand::common::g4_id_maps(edep.GetChildrenTrajectories());

    for (std::size_t ixn{}, nu_size{truth_branch.nu.size()}; ixn != nu_size; ++ixn) {
      auto const& ids = g4_ids.at(ixn);

      auto add_all = [&](std::vector<::caf::SRTrueParticle> const& particles, ::caf::TrueParticleID::PartType type) {
        for (std::size_t idx{}, particles_size{particles.size()}; idx != particles_size; ++idx) {
          auto const& p         = particles[idx];
          auto const spill_id   = ids.to_spill_id(p.G4ID);
          auto const in_tracker = p.G4ID >= 0 && std::binary_search(tracker_ids.begin(), tracker_ids.end(), spill_id);
          if (!in_tracker) {
            continue;
          }

          ::caf::TrueParticleID const id{static_cast<int>(ixn), type, static_cast<int>(idx)};
          if (is_track_like(p.pdg)) {
            auto const trj = edep.GetTrajectory(spill_id);
            tracker_reco.tracks.push_back(track_from_true(p, id, trj != edep.end() ? &*trj : nullptr));
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
