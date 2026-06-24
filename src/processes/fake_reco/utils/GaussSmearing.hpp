#pragma once

#include <common/version.h>
#include <ufw/context.hpp>

#include <edep_reader/EDEPTrajectory.h>
#include <edep_reader/EDEPTree.h>

#include <duneanaobj/StandardRecord/SRRecoParticle.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>

namespace sand {

    // namespace resolution {
    //     constexpr double energy_smearing   = 0.057;  // relative energy smearing
    //     constexpr double position_smearing = 200e-3; // position smearing in mm
    //     constexpr double momentum_smearing = 0.03;   // relative momentum smearing
    // } // namespace resolution

    // namespace res = resolution;
    enum class Var { energy, momentum, position };

    struct GaussSmearing{
        const double energy_smearing   = 0.057;  // relative energy smearing
        const double position_smearing = 200e-3; // position smearing in mm
        const double momentum_smearing = 0.03;   // relative momentum smearing

        GaussSmearing();

        template <typename T, Var V>
        T apply_smearing(const double resolution, const T& value);
    
        ufw::context::random_engine& m_random_engine() { return ufw::context::current()->engine(); };

    };

 
}