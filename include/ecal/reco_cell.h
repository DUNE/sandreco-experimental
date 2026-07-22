#pragma once

#include <common/sand.h>
#include <common/timerange.h>
#include <ecal/cell_pair.h>

// #include <cmath>

namespace sand::ecal {

  /// @brief ECal cell pair signals with reconstructed quantities.
  struct reco_cell : cell_pair {
    /// Reconstructed position [mm].
    pos_3d position = {NAN, NAN, NAN};

    /// Reconstructed particle time [ns].
    reco::timerange time;

    /// Reconstructed deposited energy [MeV].
    double e = NAN;

    /// Reconstructed light path distance to each readout face [mm].
    double d_begin = NAN;
    double d_end   = NAN;

    /// True if this reco cell was first built from a one-sided (incomplete) digit pair.
    bool originally_incomplete = false;
  };

} // namespace sand::ecal

UFW_DECLARE_UNMANAGED_DATA(sand::ecal::reco_cell);
