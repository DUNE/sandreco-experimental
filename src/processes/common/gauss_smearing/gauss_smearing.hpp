#ifndef SAND_COMMON_GAUSS_SMEARING
#define SAND_COMMON_GAUSS_SMEARING

#include <common/version.h>

#include <ufw/process.hpp>

namespace caf {
  class SRInteraction;
  class SRRecoParticle;
} // namespace caf

namespace sand::common {

  /// Interaction with vtx/Enu perturbed by a gaussian resolution
  [[nodiscard]] ::caf::SRInteraction smear_interaction(::caf::SRInteraction const& reco_ixn, double energy_res,
                                                       double x_res, double y_res, double z_res);

  /// Particle from true with E/p/start/end perturbed by a gaussian resolution
  [[nodiscard]] ::caf::SRRecoParticle smear_particle(::caf::SRRecoParticle const& reco_part, double energy_res,
                                                     double momentum_res, double x_res, double y_res, double z_res);

  /**
   * \class sand::common::gauss_smearing
   *
   * \brief Perturbs the "perfect" common reco (`in_common`) with a Gaussian smearing model, producing a
   * degraded reco (`out_common`).
   *
   * For every `caf::SRInteraction` in `in_common.ixn.sandreco`, smears `vtx` (position) and `Enu` (every
   * energy estimator collapses to the same smeared value, mirroring how `fast_reco` fills them — see
   * `smear_interaction()`), then smears `E`/`p`/`start`/`end` on every `SRRecoParticle` in
   * `ixn.part.sandreco` (see `smear_particle()`). Everything else (topology, links, `nd` branch — not
   * touched at all) is copied through unchanged.
   *
   * \subsection Configuration
   * | Parameter Name         | Type   | Unit             | Required/Default | Description |
   * |-------------------------|--------|------------------|-------------------|-------------------------------------------------------------------------------|
   * | `energy_resolution`    | double | dimensionless    | Default: 0.0      | Calorimeter-style coefficient: `sigma_E
   * = energy_resolution * sqrt(E)`, `E` in GeV. | | `momentum_resolution`  | double | dimensionless    | Default: 0.0
   * | Fractional resolution: `sigma_p = momentum_resolution * \|p\|`.                | | `x_resolution`         |
   * double | cm               | Default: 0.0      | Gaussian sigma for the x coordinate of every smeared position
   * (`vtx`/`start`/`end`). | | `y_resolution`         | double | cm               | Default: 0.0      | Gaussian sigma
   * for the y coordinate.                                         | | `z_resolution`         | double | cm | Default:
   * 0.0      | Gaussian sigma for the z coordinate.                                         |
   *
   * \subsection Dependencies
   * None; smearing draws from `ufw::context::current()->engine()`, the per-context random engine already
   * seeded by the framework — no separate dependency lookup.
   *
   * \subsection Requirements
   * |  Name        | Type                                   | Comment                                     |
   * |--------------|------------------------------------------|------------------------------------------------|
   * | `in_common`  | `sand::caf::common_reco_branch_wrapper`  | "Perfect" common reco to smear (e.g. from `fast_reco`)
   * |
   *
   * \subsection Products
   * |  Name         | Type                                   | Comment |
   * |---------------|--------------------------------------------|--------------------------------------------------------------------|
   * | `out_common`  | `sand::caf::common_reco_branch_wrapper`   | Smeared common reco: `vtx`/`Enu` per interaction,
   * `E`/`p`/`start`/`end` per particle |
   */
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
