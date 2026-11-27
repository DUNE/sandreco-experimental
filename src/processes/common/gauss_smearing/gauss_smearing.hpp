#ifndef SAND_COMMON_GAUSS_SMEARING
#define SAND_COMMON_GAUSS_SMEARING

#include <common/version.h>

#include <ufw/process.hpp>

namespace caf {
  class SRInteraction;
  class SRRecoParticle;
} // namespace caf

namespace sand {
  class particle_table;
} // namespace sand

namespace sand::common {

  [[nodiscard]] ::caf::SRInteraction smear_interaction(::caf::SRInteraction const& reco_ixn, double energy_res,
                                                       double x_res, double y_res, double z_res);

  /// Smears the momentum and the start and end positions of `reco_part`; the energy follows from the smeared
  /// momentum and the mass of `reco_part.pdg` in `table` (0 for codes not in the table).
  [[nodiscard]] ::caf::SRRecoParticle smear_particle(::caf::SRRecoParticle const& reco_part,
                                                     sand::particle_table const& table, double momentum_res,
                                                     double x_res, double y_res, double z_res);

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
