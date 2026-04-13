#include "smearer.hpp"

namespace smearer{
    void ParticleSmearer::E_smearing(::caf::SRRecoParticle& part){
        const auto E_stddev = std::sqrt(part.E) * kRes_E;
        std::normal_distribution<double> gaus(0,E_stddev);
        const auto sigma = gaus(kRng);
        part.E = std::max(0.0f, static_cast<float>(part.E+sigma));
        return;
    }
} //namespace smearer