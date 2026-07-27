#ifndef SAND_COMMON_FAST_RECO
#define SAND_COMMON_FAST_RECO

#include <common/version.h>

#include <ufw/process.hpp>

namespace sand::common {

  /**
   * \class sand::common::fast_reco
   *
   * \brief Copies/derives the "perfect" reco (`out_common`/`out_nd`) 1:1 from truth (`in_truth`), no smearing.
   *
   * Reads the `SRTruthBranch` produced by `truth_filler` and produces `SRCommonRecoBranch` (`out_common`)
   * and `SRNDBranch` (`out_nd`) for downstream CAF consumers (`caf_streamer`, `gauss_smearing`,
   * `gluckstern_smearing`). Every field is copied or derived directly from the matching truth
   * interaction/particle, with no smearing applied — `gauss_smearing`/`gluckstern_smearing` are the
   * processes that perturb this output afterwards. See README.md for the full field-by-field mapping.
   *
   * A true particle gets an `SRTrack`/`SRShower` in `nd.sand.ixn[i].tracker` only if it is track-/shower-like
   * by PDG **and** actually left hits in the tracker — `sand::subdetector_t::STT` or `::DRIFT`, whichever
   * the simulated geometry uses. Particles that left no tracker hit still get their `SRRecoParticle`, just
   * with `recoobj`/`origRecoObjType` unset, so the truth-to-reco particle indexing stays 1:1.
   *
   * \subsection Configuration
   * None; `fast_reco` takes no configuration parameters.
   *
   * \subsection Dependencies
   * | Name                | Type               | Comment                                                |
   * |---------------------|--------------------|--------------------------------------------------------|
   * | `sand::edep_reader` | context instance   | Energy deposits: which particles have tracker hits      |
   *
   * \subsection Requirements
   * | Name       | Type                              | Comment                                        |
   * |------------|-----------------------------------|------------------------------------------------|
   * | `in_truth` | `sand::caf::truth_branch_wrapper` | Truth interactions/particles (`SRTruthBranch`)  |
   *
   * \subsection Products
   * | Name         | Type                                    | Comment                                     |
   * |--------------|-----------------------------------------|---------------------------------------------|
   * | `out_common` | `sand::caf::common_reco_branch_wrapper` | vtx/dir/nuhyp/Enu + reco particles           |
   * | `out_nd`     | `sand::caf::nd_reco_branch_wrapper`     | SAND-ND tracker: tracks/showers              |
   */
  struct fast_reco : public ufw::process {
    fast_reco();
    void run() override;
  };

} // namespace sand::common

UFW_REGISTER_PROCESS(sand::common::fast_reco);

#endif
