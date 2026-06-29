#include "GaussSmearing.hpp"
// #include "caf_filler_common.hpp"

namespace sand {

    GaussSmearing::GaussSmearing() = default;

    static double smear_component(double v, double sigma, ufw::context::random_engine& rng) {
        return v + std::normal_distribution<double>(0.0, sigma)(rng);
    }

    double GaussSmearing::apply_energy_smearing(const double energy, const double en_res){
        double sigma = en_res * std::sqrt(energy);
        return smear_component(energy, sigma, random_engine());
    }


    ::caf::SRVector3D GaussSmearing::apply_pos_smearing(const ::caf::SRVector3D& pos, const double x_res, const double y_res, const double z_res){
        ::caf::SRVector3D smeared_pos;
        smeared_pos.x = smear_component(pos.x, x_res, random_engine());
        smeared_pos.y = smear_component(pos.y, y_res, random_engine());
        smeared_pos.z = smear_component(pos.z, z_res, random_engine());
        return smeared_pos;
    }

    ::caf::SRVector3D GaussSmearing::apply_momentum_smearing(const ::caf::SRLorentzVector& true_p, const double p_res){
        const double p_magnitude = std::hypot(true_p.X(), true_p.Y(), true_p.Z());
        const double p_transverse = std::hypot(true_p.Y(), true_p.Z());

        const double dip_angle = std::atan(true_p.X() / p_transverse);
        const double zy_angle = std::atan2(true_p.Y(), true_p.Z());

        double sigma = p_res * p_magnitude;
        double smeared_p = p_magnitude + std::normal_distribution<double>(0.0, sigma)(random_engine());

        return ::caf::SRVector3D{static_cast<float>(smeared_p * std::sin(dip_angle)),
                                static_cast<float>(smeared_p * std::cos(dip_angle) * std::sin(zy_angle)),
                                static_cast<float>(smeared_p * std::cos(dip_angle) * std::cos(zy_angle))};
    }

}