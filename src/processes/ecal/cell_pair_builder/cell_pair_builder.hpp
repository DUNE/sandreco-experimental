#pragma once

#include <common/sand.h>
#include <ufw/process.hpp>

namespace sand::ecal {

  class cell_pair_builder : public ufw::process {
   public:
    cell_pair_builder();

    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    /// Tolerance added to the physical begin/end time compatibility.
    ///
    /// The builder reconstructs the light-propagation distances from the
    /// begin/end PMT TDC difference and keeps a complete pair only if both
    /// distances are compatible with lying inside the cell length. This value is
    /// converted to a distance margin using the fiber light velocity, allowing
    /// small timing, calibration, or digitization fluctuations at the cell
    /// boundaries.
    ///
    /// A value of 0 means that only strictly physical pairs are kept: both
    /// reconstructed distances must be between 0 and the cell path length.
    double m_pair_time_tolerance_ns = 0.0;
    /// If true, begin-only/end-only digits that are not part of any
    /// accepted complete pair are kept as incomplete cell_pair objects.
    bool m_keep_incomplete = true;
  };

} // namespace sand::ecal

UFW_REGISTER_PROCESS(sand::ecal::cell_pair_builder)