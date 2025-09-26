//
// Created by paolo on 08/05/2025.
//

#ifndef FAKE_RECO_HPP
#define FAKE_RECO_HPP

#include <ufw/process.hpp>

#include <edep_reader/edep_reader.hpp>

#include <duneanaobj/StandardRecord/StandardRecord.h>

namespace sand::fake_reco {

    caf::SRInteraction empty_interaction_from_vertex(const EDEPTree& vertex);

    caf::SRRecoParticle
    reco_particle_from_edep_trajectory(const EDEPTrajectory& particle);

    caf::SRTrueParticle
    true_particle_from_edep_trajectory(const EDEPTrajectory& particle);

    void fill_sr_interaction(caf::SRInteraction& interaction,
                             const caf::SRRecoParticle& particle);

    void fill_sr_true_interaction(caf::SRTrueInteraction& interaction,
                                  const caf::SRTrueParticle& particle);

    class fake_reco : public ufw::process {
    public:
        fake_reco();

        void configure(const ufw::config& cfg) override;
        void run() override;
    };
} // namespace sand::fake_reco

UFW_REGISTER_PROCESS(sand::fake_reco::fake_reco);

#endif // FAKE_RECO_HPP
