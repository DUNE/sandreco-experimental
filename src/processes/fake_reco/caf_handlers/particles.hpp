//
// Created by Paolo Forni on 27/09/2025.
//

#ifndef SR_PARTICLES_HANDLERS_HPP
#define SR_PARTICLES_HANDLERS_HPP

#include <edep_reader/EDEPTrajectory.h>

#include <duneanaobj/StandardRecord/SRRecoParticle.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>

namespace sand::fake_reco {

inline caf::SRRecoParticle
reco_particle_from_edep_trajectory(const EDEPTrajectory& particle) {
    return {
        .primary = true,
        .pdg = particle.GetPDGCode(),
        .tgtA = 0, // ?
        .score = std::numeric_limits<float>::signaling_NaN(), // ?
        .E = 0, // ?
        .E_method = caf::PartEMethod::kUnknownMethod, // ?
        .p = caf::SRVector3D{particle.GetInitialMomentum().Vect()},
        .start = {}, // FIXME: waiting for the GeoManager
        // .start =
        //     caf::SRVector3D{
        //         particle.GetTrajectoryPointsVect()
        //             .front()
        //             .GetPosition()
        //             .Vect() // are these [cm]?
        //     },
        .end = {}, // FIXME: waiting for the GeoManager
        // .end = caf::SRVector3D{particle.GetTrajectoryPointsVect()
        //                            .back()
        //                            .GetPosition()
        //                            .Vect()},
        .contained = false, // ?
        .truth = {}, // ?
        .truthOverlap = {} // ?
    };
}

inline caf::SRTrueParticle
true_particle_from_edep_trajectory(const EDEPTrajectory& particle) {
    return {
        .pdg = particle.GetPDGCode(),
        .G4ID = -1, // ?
        .interaction_id = particle.GetId(), // ?
        .time = {}, // FIXME: waiting for the GeoManager
        // .time = static_cast<float>(particle.GetTrajectoryPointsVect()
        //                                .front()
        //                                .GetPosition()
        //                                .T()), // ?
        .ancestor_id = {}, // ?
        .p = particle.GetInitialMomentum(),
        .start_pos = {}, // FIXME: waiting for the GeoManager
        // .start_pos =
        //     caf::SRVector3D{
        //         particle.GetTrajectoryPointsVect()
        //             .front()
        //             .GetPosition()
        //             .Vect() // are these [cm]?
        //     },
        .end_pos = {}, // FIXME: waiting for the GeoManager
        // .end_pos = caf::SRVector3D{particle.GetTrajectoryPointsVect()
        //                                .back()
        //                                .GetPosition()
        //                                .Vect()},
        .parent = particle.GetParentId(),
        .daughters = {}, // should I get this visiting the whole
        // children tree or just the first layer?
        // FIXME: waiting for the GeoManager
        .first_process = 0,
        .first_subprocess = 0,
        .end_process = 0,
        .end_subprocess = 0
        // .first_process = static_cast<unsigned int>(
        //     particle.GetTrajectoryPointsVect().front().GetProcess()),
        // .first_subprocess = static_cast<unsigned int>(
        //     particle.GetTrajectoryPointsVect().front().GetSubprocess()),
        // .end_process = static_cast<unsigned int>(
        //     particle.GetTrajectoryPointsVect().back().GetProcess()),
        // .end_subprocess = static_cast<unsigned int>(
        //     particle.GetTrajectoryPointsVect().back().GetSubprocess())
    };
}

} // namespace sand::fake_reco

#endif // SR_PARTICLES_HANDLERS_HPP
