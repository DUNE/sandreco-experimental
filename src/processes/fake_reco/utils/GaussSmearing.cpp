#include "GaussSmearing.hpp"

namespace sand {

    GaussSmearing::GaussSmearing() = default;

    static double smear_component(double v, double sigma, ufw::context::random_engine& rng) {
        return v + std::normal_distribution<double>(0.0, sigma)(rng);
    }

    static double smear_value(double value, double sigma, ufw::context::random_engine& rng){
        return smear_component(value, sigma, rng);
    }

    static ::caf::SRVector3D smear_value(const ::caf::SRVector3D& value, const double sigma, ufw::context::random_engine& rng){
        ::caf::SRVector3D out;
        out.x = smear_component(value.x, sigma, rng);
        out.y = smear_component(value.y, sigma, rng);
        out.z = smear_component(value.z, sigma, rng);
    };

    template <typename T, Var V>
    T GaussSmearing::apply_smearing(const T& value, const double resolution) {
        double sigma;

        if constexpr (V == Var::energy){
            sigma = resolution * std::sqrt(value);
        } else if constexpr (V == Var::momentum){
            const double true_momentum = std::hypot(value.x, value.y, value.z);
            sigma = resolution * true_momentum;
        } else if constexpr (V == Var::position){
            sigma = resolution;
        } else {
            UFW_ERROR("Smearing not applied");
        }
        return smear_value(value, sigma, random_engine());
    };

  template double GaussSmearing::apply_smearing<double, Var::energy>(const double& value, const double resolution);
  template ::caf::SRVector3D GaussSmearing::apply_smearing<::caf::SRVector3D, Var::momentum>(const ::caf::SRVector3D& value, const double resolution);
  template ::caf::SRVector3D GaussSmearing::apply_smearing<::caf::SRVector3D, Var::position>(const ::caf::SRVector3D& value, const double resolution);
}