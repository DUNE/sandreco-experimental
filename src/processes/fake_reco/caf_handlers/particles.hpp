//
// Created by Paolo Forni on 27/09/2025.
//

#ifndef SR_PARTICLES_HANDLERS_HPP
#define SR_PARTICLES_HANDLERS_HPP

#include <genie_reader/genie_reader.hpp>
#include <edep_reader/EDEPTrajectory.h>

#include <duneanaobj/StandardRecord/SRRecoParticle.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>

namespace sand::fake_reco {

  inline ::caf::SRRecoParticle reco_particle_from_edep_trajectory(const EDEPTrajectory& particle) {
    return {
        .primary  = true,
        .pdg      = particle.GetPDGCode(),
        .tgtA     = 0,                                           // ?
        .score    = std::numeric_limits<float>::signaling_NaN(), // ?
        .E        = 0,                                           // ?
        .E_method = ::caf::PartEMethod::kUnknownMethod,          // ?
        .p        = ::caf::SRVector3D{particle.GetInitialMomentum().Vect()},
        .start =
            ::caf::SRVector3D{
                particle.GetTrajectoryPointsVect().front().GetPosition().Vect() // are these [cm]?
            },
        .end          = ::caf::SRVector3D{particle.GetTrajectoryPointsVect().back().GetPosition().Vect()},
        .contained    = false, // ?
        .truth        = {},    // ?
        .truthOverlap = {}     // ?
    };
  }

  inline ::caf::SRTrueParticle SRTrueParticle_from_genie(const std::size_t i, const StdHep& std_hep) {
    return {
        .pdg            = std_hep.Pdg_[i],
        .G4ID           = -1, // This function is used only for genie only particles
        .interaction_id = -1, // This will be filled by fake_reco loop
        .time           = {}, // Genie doesn't have time data
        .ancestor_id    = {}, // A particle not propagated by GEANT can't descend from a GEANT primary
        .p         = TLorentzVector{std_hep.P4_[i].Px(), std_hep.P4_[i].Py(), std_hep.P4_[i].Pz(), std_hep.P4_[i].E()},
        .start_pos = {} // Genie spatial coordinates aren't in any reference system
                        // All the other fields are for GEANT data
    };
  }

  inline ::caf::SRTrueParticle SRTrueParticle_from_edep(const EDEPTrajectory& particle) {
    const auto trajectory_points = particle.GetTrajectoryPointsVect();
    return {.pdg = particle.GetPDGCode(),
            .G4ID =
                particle.GetId(), // This is the GEANT trajectory id. The CafMaker reset this index at each interaction
            .interaction_id = {}, // This will be filled by fake_reco loop
            .time           = static_cast<float>(trajectory_points.front().GetPosition().T()), // ?
            .ancestor_id    = {}, // This will be filled by fake_reco loop
            .p              = particle.GetInitialMomentum(),
            .start_pos =
                ::caf::SRVector3D{
                    trajectory_points.front().GetPosition().Vect() // are these [cm]?
                },
            .end_pos          = ::caf::SRVector3D{trajectory_points.back().GetPosition().Vect()},
            .parent           = particle.GetParentId(),
            .daughters        = {}, // should I get this visiting the whole children tree or just the first layer?
            .first_process    = static_cast<unsigned int>(trajectory_points.front().GetProcess()),
            .first_subprocess = static_cast<unsigned int>(trajectory_points.front().GetSubprocess()),
            .end_process      = static_cast<unsigned int>(trajectory_points.back().GetProcess()),
            .end_subprocess   = static_cast<unsigned int>(trajectory_points.back().GetSubprocess())};
  }

} // namespace sand::fake_reco

#endif // SR_PARTICLES_HANDLERS_HPP
