#pragma once

#include <common/sand.h>

namespace sand::ecal {
  /// @brief ECAL photo-electron with readout channel, truth hit and arrival time.
  struct pe : sand::truth_index {
    /// Default constructor required by ROOT; channel and arrival time are invalid.
    pe() : sand::truth_index(), channel(), arrival_time(NAN) {}

    /// @param ch Readout channel receiving this photo-electron.
    /// @param idx Originating deposited-energy hit.
    /// @param tm Arrival time at the readout [ns].
    pe(channel_id ch, truth_index idx, double tm) : truth_index(idx), channel(ch), arrival_time(tm) {}

    channel_id channel;
    double arrival_time;
  };
} // namespace sand::ecal

UFW_DECLARE_UNMANAGED_DATA(sand::truth<sand::ecal::pe>);
SAND_DATA_COLLECTION(sand::ecal, pe, pes)
