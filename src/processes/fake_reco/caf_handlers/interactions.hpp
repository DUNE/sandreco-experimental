//
// Created by Paolo Forni on 27/09/2025.
//

#ifndef SR_INTERACTIONS_HANDLERS_HPP
#define SR_INTERACTIONS_HANDLERS_HPP

#include <edep_reader/EDEPTree.h>

#include <duneanaobj/StandardRecord/SRInteraction.h>
#include <duneanaobj/StandardRecord/SRTrueInteraction.h>

namespace sand::fake_reco {

inline caf::SRInteraction
empty_interaction_from_vertex(const EDEPTree& vertex) {
    caf::SRInteraction interaction{};
    // TODO: get the id from genie
    // interaction.id = ...;
    // FIXME: waiting for the GeoManager
    // interaction.vtx =
    // caf::SRVector3D{vertex.GetTrajectoryPointsVect()[0].GetPosition().Vect()};
    return interaction;
}

inline void fill_sr_interaction(caf::SRInteraction& interaction,
                                const caf::SRRecoParticle& particle) {
    interaction.part.sandreco.push_back(particle);
    interaction.part.nsandreco++;
}

inline void fill_sr_true_interaction(caf::SRTrueInteraction& interaction,
                                     const caf::SRTrueParticle& particle) {
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
