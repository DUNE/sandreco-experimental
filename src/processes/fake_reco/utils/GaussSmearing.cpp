#include "GaussSmearing.hpp"

namespace sand {

    GaussSmearing::GaussSmearing() = default;

    template <typename T, Var V>
    T GaussSmearing::apply_smearing(const double resolution, const T& value) {
        T smeared_value;
        if constexpr (V == Var::energy){
            const double sigma_en = resolution * std::sqrt(value);
            std::normal_distribution<double> absolute_error(0.0, sigma_en);
            double ran_value = absolute_error(m_random_engine());
            smeared_value = value + ran_value;
        } else if constexpr (V == Var::momentum){
            const double true_momentum = std::hypot(value.x, value.y, value.z);
            const double sigma_mom = resolution * true_momentum;
            std::normal_distribution<double> absolute_error(0.0, sigma_mom);
            smeared_value.x = value.x + absolute_error(m_random_engine());
            smeared_value.y = value.y + absolute_error(m_random_engine());
            smeared_value.z = value.z + absolute_error(m_random_engine());
        } else if constexpr (V == Var::position){
            std::normal_distribution<double> absolute_error(0.0, resolution);
            smeared_value.x = value.x + absolute_error(m_random_engine());
            smeared_value.y = value.y + absolute_error(m_random_engine());
            smeared_value.z = value.z + absolute_error(m_random_engine());
        } else {
            UFW_ERROR("Smearing not applied");
        }
        return smeared_value;
    };

  template double GaussSmearing::apply_smearing<double, Var::energy>(const double resolution, const double& value);
  template ::caf::SRVector3D GaussSmearing::apply_smearing<::caf::SRVector3D, Var::momentum>(const double resolution, const ::caf::SRVector3D& value);
  template ::caf::SRVector3D GaussSmearing::apply_smearing<::caf::SRVector3D, Var::position>(const double resolution, const ::caf::SRVector3D& value);
}