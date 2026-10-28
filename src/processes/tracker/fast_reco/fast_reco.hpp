#ifndef SAND_TRACKER_FAST_RECO
#define SAND_TRACKER_FAST_RECO

#include <edep_reader/edep_reader.hpp>

#include <ufw/config.hpp>
#include <ufw/process.hpp>

#include <duneanaobj/StandardRecord/SRTrack.h>
#include <duneanaobj/StandardRecord/SRShower.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>

#include <vector>

namespace sand::tracker {

  /// @return the hits of a trajectory in the tracker (STT or drift chambers), sorted by start time.
  /// Takes the hit map rather than the trajectory, so that it can be fed hand-made hits.
  [[nodiscard]] std::vector<EDEPHit> tracker_hits(EDEPHitsMap const& hit_map);

  /// @return the energy [MeV] deposited by `hits`.
  [[nodiscard]] double total_energy_deposit(std::vector<EDEPHit> const& hits);

  /// \brief Perfect track from the particle's tracker hits (sorted by time): position, time, length and visible
  /// energy as the tracker sees them; charge from the pdg; E and directions still from the truth.
  /// `points` are the particle's trajectory points per subdetector (EDEPTrajectory::GetTrajectoryPoints), for the end
  /// direction; with fewer than two points, enddir == dir.
  [[nodiscard]] ::caf::SRTrack track_from_hits(std::vector<EDEPHit> const& hits, EDEPTrajectoryPoints const& points,
                                               ::caf::SRTrueParticle const& true_part, ::caf::TrueParticleID const& id);

  /// \brief Perfect shower from the particle's tracker hits (sorted by time): start, time and visible energy as the
  /// tracker sees them; direction still from the truth.
  [[nodiscard]] ::caf::SRShower shower_from_hits(std::vector<EDEPHit> const& hits,
                                                 ::caf::SRTrueParticle const& true_part,
                                                 ::caf::TrueParticleID const& id);

  /// \brief Perfect tracker reco: one SRTrack/SRShower per true particle seen by the tracker.
  ///
  /// Config:
  /// - `min_energy_deposit` [MeV, default 0]: minimum energy a particle must deposit in the tracker to get an object.
  ///   Single sub-threshold deposits that Geant4 attributes to photons are a few keV (at most ~30 keV in the test
  ///   sample), while the softest real tracks deposit ~50 keV or more.
  class fast_reco : public ufw::process {
    double m_min_energy_deposit{};

   public:
    fast_reco();
    void configure(ufw::config const& cfg) override;
    void run() override;
  };

} // namespace sand::tracker

UFW_REGISTER_PROCESS(sand::tracker::fast_reco);

#endif
