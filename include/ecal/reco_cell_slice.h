#pragma once

#include <common/data.h>
#include <ecal/reco_cell.h>

#include <vector>

namespace sand::ecal {

  /// @brief Managed container for collections of slices of reco_cell objects.
  ///
  /// Each slice contains reconstructed ECal cells built from the corresponding
  /// cell-pair slice.
  struct reco_cell_slices_container : managed_data_base {
    using slice = std::vector<reco_cell>;
    using slice_collection = std::vector<slice>;

    slice_collection collection;
  };

} // namespace sand::ecal

UFW_DECLARE_MANAGED_DATA(sand::ecal::reco_cell_slices_container);
