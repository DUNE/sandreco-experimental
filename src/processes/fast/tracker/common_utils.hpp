#ifndef SAND_FAST_RECO_COMMON_UTILS_HPP
#define SAND_FAST_RECO_COMMON_UTILS_HPP

#include <edep_reader/edep_reader.hpp>
#include <spill_maps/spill_maps.hpp>

#include <ufw/utils.hpp>

#include <duneanaobj/StandardRecord/SRShower.h>
#include <duneanaobj/StandardRecord/SRTrack.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>
#include <duneanaobj/StandardRecord/SRVector3D.h>
#include <duneanaobj/StandardRecord/SRTruthBranch.h>

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <vector>

namespace sand::fast_reco {

  /// What one trajectory leaves in a set of subdetector volumes.
  ///
  /// A self-contained value: hits and points are copies, nothing refers into the EDEPTree it was built from.
  struct VolumeHits {
    std::vector<EDEPHit> hits;                      ///< hits in the volumes, ordered by start time
    std::optional<EDEPTrajectoryPoint> entry;       ///< first trajectory point in the volumes, if any
    std::optional<EDEPTrajectoryPoint> exit;        ///< last one
    std::optional<EDEPTrajectoryPoint> before_exit; ///< the one before `exit`
  };

  // `volumes` would be a std::span<subdetector_t const> in C++20; std::initializer_list is the C++17 stand-in
  VolumeHits volume_hits_in(EDEPHitsMap const& hit_map, EDEPTrajectoryPoints const& points,
                            std::initializer_list<subdetector_t> volumes);

  /// volume_hits_in() on the hits and trajectory points of `trj`.
  VolumeHits volume_hits_through(EDEPTrajectory const& trj, std::initializer_list<subdetector_t> volumes);

  /// Total energy deposited by the hits [MeV].
  double energy_deposit(VolumeHits const& volume_hits);

  /// Sum of the hit lengths [mm]: the path length in the volumes, not the chord.
  double track_length(VolumeHits const& volume_hits);

  /// Momentum direction at `entry`; direction of the first hit if there is no entry point or the particle is stopped.
  ::caf::SRVector3D entry_direction(VolumeHits const& volume_hits);

  /// Momentum direction at `exit`; for a particle stopping in the volumes, from `before_exit` to `exit`; otherwise
  /// direction of the last hit.
  ::caf::SRVector3D exit_direction(VolumeHits const& volume_hits);

  /// SRTrack of a particle from what it left in the volumes: position, time, length and Evis from the hits,
  /// directions at entry and exit, E [MeV] at the entry point (NaN if there is none). Needs at least one hit.
  ::caf::SRTrack track_from(VolumeHits const& volume_hits, ::caf::SRTrueParticle const& true_part,
                            ::caf::TrueParticleID const& id);

  /// SRShower of a particle from what it left in the volumes: start, time and Evis from the hits, direction at
  /// entry. Needs at least one hit.
  ::caf::SRShower shower_from(VolumeHits const& volume_hits, ::caf::TrueParticleID const& id);

  /// True for the long-lived charged particles that leave a track: mu±, pi±, K±, p and pbar.
  bool is_track_like(int pdg);

  /// True for the electromagnetic particles that start a shower: e±, gamma, pi0.
  bool is_shower_like(int pdg);

  /// Calls `f(true_particle, id, trajectory)` for every true particle of the spill: interaction by interaction,
  /// primaries before secondaries, in the order of the truth branch. UFW_ERROR if a true particle has no edep-sim
  /// trajectory.
  template <typename F>
  void for_each_true_particle(::caf::SRTruthBranch const& truth, edep_reader const& edep, spill_maps const& maps,
                              F&& f) {
    for (std::size_t ixn{}, nu_size{truth.nu.size()}; ixn != nu_size; ++ixn) {
      auto const& ids = maps.at(ixn);

      auto visit = [&](std::vector<::caf::SRTrueParticle> const& particles, ::caf::TrueParticleID::PartType type) {
        for (std::size_t idx{}, particles_size{particles.size()}; idx != particles_size; ++idx) {
          auto const& p       = particles[idx];
          auto const spill_id = ids.to_spill_id(p.G4ID);
          auto const trj      = edep.GetTrajectory(spill_id);
          if (trj == edep.end()) {
            UFW_ERROR("nu[{}]: true particle G4ID {} (spill id {}) has no edep-sim trajectory", ixn, p.G4ID, spill_id);
          }
          f(p, ::caf::TrueParticleID{static_cast<int>(ixn), type, static_cast<int>(idx)}, *trj);
        }
      };
      visit(truth.nu[ixn].prim, ::caf::TrueParticleID::kPrimary);
      visit(truth.nu[ixn].sec, ::caf::TrueParticleID::kSecondary);
    }
  }

} // namespace sand::fast_reco

#endif
