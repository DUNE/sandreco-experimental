#pragma once

#include <common/version.h>
#include <ufw/context.hpp>

#include <edep_reader/EDEPTrajectory.h>
#include <edep_reader/EDEPTree.h>

#include <duneanaobj/StandardRecord/SRRecoParticle.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>

namespace sand {

    struct GaussSmearing{
        const double energy_resolution   = 0.057;  // relative energy smearing
        const double pos_resolution      = 200e-3; // position smearing in mm
        const double momentum_resolution = 0.03;   // relative momentum smearing

        GaussSmearing();

        double apply_energy_smearing(const double energy, const double en_res);
        ::caf::SRVector3D apply_pos_smearing(const ::caf::SRVector3D& pos, const double x_res, const double y_res, const double z_res);
        ::caf::SRVector3D apply_momentum_smearing(const ::caf::SRLorentzVector& true_p, const double p_res);
    
        ufw::context::random_engine& random_engine() { return ufw::context::current()->engine(); };

    };

 
}