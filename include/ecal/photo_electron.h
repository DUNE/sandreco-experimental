#pragma once

#include <ufw/data.hpp>
#include <common/sand.h>
#include <common/truth.h>

namespace sand::ecal {
  /// @brief Photo-electron data container for ECAL
  ///
  /// The pes struct represents a managed data container that stores photo-electron
  /// information for multiple channels in the electromagnetic calorimeter. Each photo-electron
  /// is associated with a channel and contains truth index and arrival time information.
  struct pes : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    /// @brief Individual photo-electron with arrival time information
    ///
    /// A photo-electron extends the base truth class with an additional arrival time
    /// measurement, useful for timing-based analysis in the calorimeter.
    struct pe : public sand::truth {
      /// @brief Default constructor initializing pe with NaN arrival time
      pe() : sand::truth(), arrival_time(NAN) {};

      /// @brief Parameterized constructor
      /// @param idx Truth index identifier for the photo-electron
      /// @param tm Arrival time of the photo-electron (in nanoseconds or appropriate unit)
      pe(truth_index idx, double tm) : sand::truth(idx), arrival_time(tm) {};

      /// @brief Arrival time of the photo-electron
      double arrival_time;
    };

    /// @brief Map associating channel IDs to collections of photo-electrons
    using pe_collection = std::map<channel_id, std::vector<pe>>;

    /// @brief Collection of photo-electrons organized by channel
    pe_collection collection;
  };
} // namespace sand::ecal

UFW_DECLARE_MANAGED_DATA(sand::ecal::pes);
