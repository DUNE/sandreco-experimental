#ifndef SAND_COMMON_GAUSS_SMEARING
#define SAND_COMMON_GAUSS_SMEARING

#include <common/version.h>

#include <ufw/process.hpp>

namespace caf {
  class SRInteraction;
  class SRRecoParticle;
} // namespace caf

namespace sand::common {

  [[nodiscard]] ::caf::SRInteraction smear_interaction(::caf::SRInteraction const& reco_ixn, double energy_res,
                                                       double x_res, double y_res, double z_res);

  [[nodiscard]] ::caf::SRRecoParticle smear_particle(::caf::SRRecoParticle const& reco_part, double energy_res,
                                                     double momentum_res, double x_res, double y_res, double z_res);

  class gauss_smearing : public ufw::process {
    double m_energy_res{};
    double m_momentum_res{};
    double m_x_res{};
    double m_y_res{};
    double m_z_res{};

   public:
    gauss_smearing();
    void configure(ufw::config const& cfg) override;
    void run() override;
  };

} // namespace sand::common

UFW_REGISTER_PROCESS(sand::common::gauss_smearing);

#endif
