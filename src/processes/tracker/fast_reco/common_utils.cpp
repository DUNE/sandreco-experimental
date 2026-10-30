#include "common_utils.hpp"

#include <ufw/utils.hpp>

#include <TDatabasePDG.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numeric>

namespace sand::fast_reco {

  namespace {
    std::vector<EDEPHit> hits_in(EDEPHitsMap const& hit_map, std::initializer_list<subdetector_t> volumes) {
      std::vector<EDEPHit> hits;
      for (auto const vol : volumes) {
        if (auto const it = hit_map.find(vol); it != hit_map.end()) {
          hits.insert(hits.end(), it->second.begin(), it->second.end());
        }
      }
      std::stable_sort(hits.begin(), hits.end(),
                       [](EDEPHit const& a, EDEPHit const& b) { return a.GetStart().T() < b.GetStart().T(); });
      return hits;
    }

    struct ExtremePoints {
      EDEPTrajectoryPoint const* first{};
      EDEPTrajectoryPoint const* last{};
      EDEPTrajectoryPoint const* before_last{};
    };

    ExtremePoints extreme_points_in(EDEPTrajectoryPoints const& points, std::initializer_list<subdetector_t> volumes) {
      ExtremePoints ep;
      for (auto const vol : volumes) {
        auto const it = points.find(vol);
        if (it == points.end()) {
          continue;
        }
        for (auto const& point : it->second) {
          auto const t = point.GetPosition().T();
          if (ep.first == nullptr || t < ep.first->GetPosition().T()) {
            ep.first = &point;
          }
          // A point in a volume that is both STT and DRIFT is stored once per subdetector: skip the copy, or the
          // second-to-last point would coincide with the last one.
          if (ep.last != nullptr && point == *ep.last) {
            continue;
          }
          if (ep.last == nullptr || t >= ep.last->GetPosition().T()) {
            ep.before_last = ep.last;
            ep.last        = &point;
          } else if (ep.before_last == nullptr || t >= ep.before_last->GetPosition().T()) {
            ep.before_last = &point;
          }
        }
      }
      return ep;
    }

    ::caf::SRVector3D normalize_to_direction(float px, float py, float pz) {
      float const mag = std::sqrt(px * px + py * py + pz * pz);
      return (mag > 0.f) ? ::caf::SRVector3D{px / mag, py / mag, pz / mag} : ::caf::SRVector3D{0.f, 0.f, 0.f};
    }

    double constexpr stopped_momentum{1e-6}; // MeV

    bool is_stopped(sand::mom_3d const& p) { return std::hypot(p.x(), p.y(), p.z()) <= stopped_momentum; }

    ::caf::SRVector3D direction_of(sand::mom_3d const& p) {
      return normalize_to_direction(static_cast<float>(p.x()), static_cast<float>(p.y()), static_cast<float>(p.z()));
    }

    /// Direction from `a` to `b`.
    ::caf::SRVector3D direction_from_to(sand::vec_4d const& a, sand::vec_4d const& b) {
      return normalize_to_direction(static_cast<float>(b.X() - a.X()), static_cast<float>(b.Y() - a.Y()),
                                    static_cast<float>(b.Z() - a.Z()));
    }

    ::caf::SRVector3D position_cm(sand::vec_4d const& v) {
      return {static_cast<float>(v.X() * 0.1), static_cast<float>(v.Y() * 0.1), static_cast<float>(v.Z() * 0.1)};
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

    double mass_mev(int pdg) {
      auto const* p = TDatabasePDG::Instance()->GetParticle(pdg);
      return p != nullptr ? p->Mass() * 1000. : 0.; // TDatabasePDG masses are in GeV
    }

  } // namespace

  VolumeHits volume_hits_in(EDEPHitsMap const& hit_map, EDEPTrajectoryPoints const& points,
                            std::initializer_list<subdetector_t> volumes) {
    auto const ep = extreme_points_in(points, volumes);

    auto const copy = [](EDEPTrajectoryPoint const* p) {
      return p != nullptr ? std::optional<EDEPTrajectoryPoint>{*p} : std::nullopt;
    };
    return {hits_in(hit_map, volumes), copy(ep.first), copy(ep.last), copy(ep.before_last)};
  }

  VolumeHits volume_hits_through(EDEPTrajectory const& trj, std::initializer_list<subdetector_t> volumes) {
    return volume_hits_in(trj.GetHitMap(), trj.GetTrajectoryPoints(), volumes);
  }

  double energy_deposit(VolumeHits const& volume_hits) {
    auto const& hits = volume_hits.hits;
    return std::accumulate(hits.begin(), hits.end(), 0.,
                           [](double sum, EDEPHit const& hit) { return sum + hit.GetEnergyDeposit(); });
  }

  double track_length(VolumeHits const& volume_hits) {
    auto const& hits = volume_hits.hits;
    return std::accumulate(hits.begin(), hits.end(), 0.,
                           [](double sum, EDEPHit const& hit) { return sum + hit.GetTrackLength(); });
  }

  ::caf::SRVector3D entry_direction(VolumeHits const& volume_hits) {
    if (volume_hits.entry && !is_stopped(volume_hits.entry->GetMomentum())) {
      return direction_of(volume_hits.entry->GetMomentum());
    }
    UFW_ASSERT(!volume_hits.hits.empty(), "entry_direction: no trajectory point and no hit in the volumes");
    auto const& hit = volume_hits.hits.front();
    return direction_from_to(hit.GetStart(), hit.GetStop());
  }

  ::caf::SRVector3D exit_direction(VolumeHits const& volume_hits) {
    if (volume_hits.exit && !is_stopped(volume_hits.exit->GetMomentum())) {
      return direction_of(volume_hits.exit->GetMomentum());
    }
    if (volume_hits.exit && volume_hits.before_exit) {
      return direction_from_to(volume_hits.before_exit->GetPosition(), volume_hits.exit->GetPosition());
    }
    UFW_ASSERT(!volume_hits.hits.empty(), "exit_direction: no trajectory point and no hit in the volumes");
    auto const& hit = volume_hits.hits.back();
    return direction_from_to(hit.GetStart(), hit.GetStop());
  }

  ::caf::SRTrack track_from(VolumeHits const& volume_hits, ::caf::SRTrueParticle const& true_part,
                            ::caf::TrueParticleID const& id) {
    UFW_ASSERT(!volume_hits.hits.empty(), "track_from: a track needs at least one hit");

    auto const& first = volume_hits.hits.front().GetStart();
    auto const& last  = volume_hits.hits.back().GetStop();

    ::caf::SRTrack track{};

    // from the hits
    track.start  = position_cm(first);
    track.end    = position_cm(last);
    track.time   = first.T();
    track.len_cm = static_cast<float>(track_length(volume_hits) * 0.1);
    track.Evis   = static_cast<float>(energy_deposit(volume_hits)); // MeV

    // from the trajectory points
    track.dir    = entry_direction(volume_hits);
    track.enddir = exit_direction(volume_hits);
    if (volume_hits.entry) {
      auto const& p = volume_hits.entry->GetMomentum(); // MeV
      track.E       = static_cast<float>(std::sqrt(p.Mag2() + std::pow(mass_mev(true_part.pdg), 2)));
    }

    track.charge = charge_from_pdg(true_part.pdg);
    track.qual   = 1.f;

    track.truth.push_back(id);
    track.truthOverlap.push_back(1.f);

    return track;
  }

  ::caf::SRShower shower_from(VolumeHits const& volume_hits, ::caf::TrueParticleID const& id) {
    UFW_ASSERT(!volume_hits.hits.empty(), "shower_from: a shower needs at least one hit");

    auto const& first = volume_hits.hits.front().GetStart();

    ::caf::SRShower shower{};

    shower.start     = position_cm(first);
    shower.time      = first.T();
    shower.Evis      = static_cast<float>(energy_deposit(volume_hits)); // MeV
    shower.direction = entry_direction(volume_hits);

    shower.truth.push_back(id);
    shower.truthOverlap.push_back(1.f);

    return shower;
  }

  bool is_track_like(int pdg) {
    int const abs_pdg = std::abs(pdg);
    return abs_pdg == 13 || abs_pdg == 211 || abs_pdg == 321 || abs_pdg == 2212;
  }

  bool is_shower_like(int pdg) {
    int const abs_pdg = std::abs(pdg);
    return abs_pdg == 11 || abs_pdg == 22 || abs_pdg == 111;
  }

} // namespace sand::fast_reco
