#ifndef SAND_COMMON_FAST_RECO_DETAILS_HPP
#define SAND_COMMON_FAST_RECO_DETAILS_HPP

#include <duneanaobj/StandardRecord/SREnums.h>

#include <vector>

namespace caf {
  class SRVector3D;
  class SRNeutrinoHypothesisBranch;
  class SRTrueInteraction;
  class SRDirectionBranch;
  class SRNeutrinoEnergyBranch;
  class SRRecoParticlesBranch;
  class SRRecoParticle;
  class SRTrueParticle;
  class SRShower;
  class SRTrack;
  class SRTracker;
} // namespace caf

namespace sand {

  class edep_reader;

  namespace common::reco_details {

    struct ParticleSlot {
      ::caf::TrueParticleID id; ///< the true particle this slot stands for
      int part_idx;             ///< its index in caf::SRRecoParticlesBranch::sandreco
      int track_idx  = -1;      ///< its index in caf::SRTracker::tracks, or -1 if it gets no track
      int shower_idx = -1;      ///< its index in caf::SRTracker::showers, or -1 if it gets no shower
    };

    using ParticleSlots = std::vector<ParticleSlot>;

    using TrackerG4IDs = std::vector<int>;

    /// \brief Collects the G4IDs of every trajectory with hits in the tracker.
    [[nodiscard]] TrackerG4IDs tracker_g4ids_from_edep(sand::edep_reader const& tree);

    /// \brief Perfect classifier: one-hot CVN scores on the true interaction class.
    [[nodiscard]] ::caf::SRNeutrinoHypothesisBranch
    neutrino_hypothesis_from_true(::caf::SRTrueInteraction const& true_ixn);

    /// \brief Perfect direction hypothesis: every estimator collapses to the true neutrino direction.
    [[nodiscard]] ::caf::SRDirectionBranch direction_from_true(::caf::SRTrueInteraction const& true_ixn);

    /// \brief Perfect neutrino energy hypothesis: every estimator collapses to the true energy.
    [[nodiscard]] ::caf::SRNeutrinoEnergyBranch energy_from_true(::caf::SRTrueInteraction const& true_ixn);

    /// \brief Perfect reco particle: every field copied or derived 1:1 from the true particle.
    [[nodiscard]] ::caf::SRRecoParticle reco_particle_from_true(::caf::SRTrueParticle const& true_part,
                                                                ::caf::TrueParticleID const& id);

    /// \brief Perfect track, for a track-like particle that has tracker hits.
    [[nodiscard]] ::caf::SRTrack track_from_true(::caf::SRTrueParticle const& true_part,
                                                 ::caf::TrueParticleID const& id);

    /// \brief Perfect shower, for a shower-like particle that has tracker hits.
    [[nodiscard]] ::caf::SRShower shower_from_true(::caf::SRTrueParticle const& true_part,
                                                   ::caf::TrueParticleID const& id);

    /// \brief Indexes every true particle of an interaction, applying the tracker gate.
    [[nodiscard]] ParticleSlots particle_slots_from_true(::caf::SRTrueInteraction const& true_ixn,
                                                         TrackerG4IDs const& tracker_ids, int ixn_idx);

    /// \brief Perfect reco particles: one caf::SRRecoParticle per true `prim`/`sec`, cross-links resolved.
    [[nodiscard]] ::caf::SRRecoParticlesBranch reco_particles_from_true(::caf::SRTrueInteraction const& true_ixn,
                                                                        TrackerG4IDs const& tracker_ids, int ixn_idx);

    /// \brief Perfect SAND tracker: one caf::SRTrack/caf::SRShower per particle that passed the gate.
    [[nodiscard]] ::caf::SRTracker sand_tracker_from_true(::caf::SRTrueInteraction const& true_ixn,
                                                          TrackerG4IDs const& tracker_ids, int ixn_idx);

  } // namespace common::reco_details
} // namespace sand

#endif
