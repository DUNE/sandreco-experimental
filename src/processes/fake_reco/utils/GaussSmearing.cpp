#include "GaussSmearing.hpp"

namespace sand {

    GaussSmearing::GaussSmearing() = default;

    template <typename T, Var V>
    T GaussSmearing::apply_smearing(const double resolution, const T& value) {

        auto smear_scalar = [&](double v, double sigma){
            return v + std::normal_distribution<double>(0.0, sigma)(m_random_engine());
        };

        auto smear_vect = [&](T& in, T& out v, double sigma){
            out.x = smear_scalar(in.x, sigma);
            out.y = smear_scalar(in.y, sigma);
            out.z = smear_scalar(in.z, sigma);
        };

        T smeared_value;

        if constexpr (V == Var::energy){
            const double sigma_en = resolution * std::sqrt(value);
            smeared_value = smear_scalar(value, sigma_en);
        } else if constexpr (V == Var::momentum){
            const double true_momentum = std::hypot(value.x, value.y, value.z);
            const double sigma_mom = resolution * true_momentum;
            smeared_value = smear_vect(value, sigma_mom);
        } else if constexpr (V == Var::position){
            smeared_value = smear_vect(value, resolution);
        } else {
            UFW_ERROR("Smearing not applied");
        }
        return smeared_value;
    };

  template double GaussSmearing::apply_smearing<double, Var::energy>(const double resolution, const double& value);
  template ::caf::SRVector3D GaussSmearing::apply_smearing<::caf::SRVector3D, Var::momentum>(const double resolution, const ::caf::SRVector3D& value);
  template ::caf::SRVector3D GaussSmearing::apply_smearing<::caf::SRVector3D, Var::position>(const double resolution, const ::caf::SRVector3D& value);
}