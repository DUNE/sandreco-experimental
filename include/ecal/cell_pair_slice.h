#pragma once

#include <vector>

#include <common/data.h>
#include <ecal/cell_pair.h>

namespace sand::ecal {

  /// @brief Managed container for collections of slices of cell_pair objects.
  ///
  /// Each slice contains the cell-pair hypotheses reconstructed from the
  /// corresponding digit slice.
  struct cell_pair_slices_container : managed_data_base {
    using slice = std::vector<cell_pair>;
    using slice_collection = std::vector<slice>;

    slice_collection collection;
  };

} // namespace sand::ecal

UFW_DECLARE_MANAGED_DATA(sand::ecal::cell_pair_slices_container);
