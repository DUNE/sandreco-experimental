#pragma once

#include <common/version.h>

#include <ufw/process.hpp>

namespace sand::ecal {

  /// Builds ECAL preclusters using geometry neighbours within each digit window.
  ///
  /// Requirements: `reco_cells` (sand::ecal::reco_cells), `timeranges` (sand::reco::timeranges).
  /// Product: `clusters` (sand::ecal::clusters).
  ///
  /// Use the ordered, disjoint windows from cell_pair_builder. Both digit TDCs must
  /// belong to the same window; reconstructed times determine cluster observables.
  /// Output is a flat collection; each cluster retains its source digit_window.
  ///
  /// Accept complete, unambiguous reco_cells with E > 0; exclude originally incomplete
  /// pairs. Geometry neighbours define connectivity, including diagonals and links
  /// across modules, without additional position or pairwise time cuts.
  /// Recovery, splitting, merging and fitting are separate stages.
  ///
  /// | Parameter | Default | Description |
  /// |-----------|---------|-------------|
  /// | print_output | false | Log cluster membership and reconstructed properties. |
  class preclustering : public ufw::process {
   public:
    preclustering();

    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    bool m_print_output = false;
  };

} // namespace sand::ecal

UFW_REGISTER_PROCESS(sand::ecal::preclustering)
