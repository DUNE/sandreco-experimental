#pragma once

#include <duneanaobj/StandardRecord/SRInteraction.h>
#include <duneanaobj/StandardRecord/SRRecoParticle.h>
#include <duneanaobj/StandardRecord/SRShower.h>
#include <duneanaobj/StandardRecord/SRTrack.h>
#include <duneanaobj/StandardRecord/SRTrueInteraction.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>

#include <random>
#include <cmath>

namespace smearer {
    class ParticleSmearer {
        private:
        std::mt19937 kRng;
        const float kRes_E = 0.057f; //sigma /E = 5.7% / sqrt(E) in gev

        public:
        inline ParticleSmearer(unsigned int seed = std::random_device{}()): kRng(seed) {}
        void E_smearing(::caf::SRRecoParticle& part);

    };

} //namespace smearer