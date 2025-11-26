//
// Created by Paolo Forni on 27/09/2025.
//

#ifndef SR_INTERACTIONS_HANDLERS_HPP
#define SR_INTERACTIONS_HANDLERS_HPP

#include <edep_reader/EDEPTree.h>
#include <genie_reader/GenieWrapper.h>

#include <duneanaobj/StandardRecord/SRInteraction.h>
#include <duneanaobj/StandardRecord/SRTrueInteraction.h>

namespace sand::fake_reco {
    inline caf::SRInteraction
    empty_interaction_from_vertex(const EDEPTree &vertex) {
        caf::SRInteraction interaction{};
        // TODO: get the id from genie
        // interaction.id = ...;
        // FIXME: waiting for the GeoManager
        // interaction.vtx =
        // caf::SRVector3D{vertex.GetTrajectoryPointsVect()[0].GetPosition().Vect()};
        return interaction;
    }

    inline void fill_sr_interaction(caf::SRInteraction &interaction,
                                    const caf::SRRecoParticle &particle) {
        interaction.part.sandreco.push_back(particle);
        interaction.part.nsandreco++;
    }

    inline void initialize_SRTrueInteraction(caf::SRTrueInteraction &interaction,
                                             const GenieWrapper &genie) {
        if (!genie.nuParent_) {
            UFW_ERROR("The gRooTraker doesn't contain nu parent informations");
        }
        const auto& nuParent = genie.nuParent_.value();
        // interaction.id = ?
        // interaction.genieIdx = ?
        interaction.pdg = nuParent.Pdg_;
        // fill this for similarity with FD, but no oscillations
        //  (comment from https://github.com/DUNE/ND_CAFMaker/blob/972f11bc5b69ea1f595e14ed16e09162f512011e/src/truth/FillTruth.cxx#L171C46-L171C102)
        interaction.pdgorig = interaction.pdg;
        // interaction.iscc = nuParent.
        // interaction.mode = genie
        // interaction.targetPDG = genie
        // interaction.hitnuc = genie

        // todo: get this from Hugh G or somebody who will get it right
        //  (comment from https://github.com/DUNE/ND_CAFMaker/blob/972f11bc5b69ea1f595e14ed16e09162f512011e/src/truth/FillTruth.cxx#L178C5-L178C67)
        // interaction.removalE = ?
    }

    inline void fill_sr_true_interaction(caf::SRTrueInteraction &interaction,
                                         const caf::SRTrueParticle &particle) {
        if (particle.parent == -1) {
            interaction.prim.push_back(particle);
            interaction.nprim++;
        } else {
            interaction.sec.push_back(particle);
            interaction.nsec++;
        }
        // Should we consider e.g. pi0 100111, ... too?
        switch (particle.pdg) {
            case 2212:
                interaction.nproton++;
                break;
            case 2112:
                interaction.nneutron++;
                break;
            case 111:
                interaction.npi0++;
                break;
            case 211:
                interaction.npip++;
                break;
            default:
                break;
        }
    }
} // namespace sand::fake_reco
#endif // SR_INTERACTIONS_HANDLERS_HPP
