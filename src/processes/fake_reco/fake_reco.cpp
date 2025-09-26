//
// Created by paolo on 08/05/2025.
//

#include <ufw/factory.hpp>

#include <duneanaobj/StandardRecord/StandardRecord.h>
// #include <duneanaobj/StandardRecord/SRTrueParticle.h>
// #include <duneanaobj/StandardRecord/SRRecoParticle.h>

#include <edep_reader/edep_reader.hpp>

#include "fake_reco.hpp"

namespace sand::fake_reco {

    caf::SRRecoParticle
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

    caf::SRTrueParticle
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


    fake_reco::fake_reco() : process{{}, {}} {}

    void fake_reco::configure(const ufw::config& cfg) {
        process::configure(cfg);
    }

    void fake_reco::run() {
        // Read input
        const auto& inputTree = get<edep_reader>();

        // Just logging some values
        std::size_t childrenTrajectoriesCount =
            inputTree.GetChildrenTrajectories().size();

        UFW_DEBUG("Reading root trajectory with id: {}", inputTree.GetId());
        UFW_DEBUG("Children trajectories foud: {}", childrenTrajectoriesCount);

        caf::SRTrueInteraction true_interaction{};

#ifndef NDEBUG
        size_t particle_number = 0;
#endif

        for (const auto& particle : inputTree) {
            UFW_DEBUG("Parsing particle number {}", particle_number++);

            caf::SRRecoParticle recoParticle =
                reco_particle_from_edep_trajectory(particle);

            caf::SRTrueParticle true_particle =
                true_particle_from_edep_trajectory(particle);

            UFW_DEBUG("Children trajectory id: {}", particle.GetId());
            UFW_DEBUG("Children trajectory PDG code: {}",
                      particle.GetPDGCode());
            UFW_DEBUG("Interaction number: {}",
                      particle.GetInteractionNumber());
            UFW_DEBUG("Reaction generated by the child trajectory: {}",
                      particle.GetReaction());

            if (true_particle.parent == -1) {
                true_interaction.prim.push_back(true_particle);
                true_interaction.nprim++;
            } else {
                true_interaction.sec.push_back(true_particle);
                true_interaction.nsec++;
            }
            // Should we consider e.g. pi0 100111, ... too?
            switch (true_particle.pdg) {
            case 2212:
                true_interaction.nproton++;
                break;
            case 2112:
                true_interaction.nneutron++;
                break;
            case 111:
                true_interaction.npi0++;
                break;
            case 211:
                true_interaction.npip++;
                break;
            default:
                break;
            }
        }

    }
} // namespace sand::fake_reco

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::fake_reco::fake_reco);
