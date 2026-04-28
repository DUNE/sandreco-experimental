#pragma once

#include <vector>

#include <common/data.h>
#include <ecal/cell_pair.h>

namespace sand::ecal {

  /// @brief Managed container for collections of slices of cell_pair objects.
  struct cell_pair_slices_container : managed_data_base {
    using cell_pair_slice = std::vector<cell_pair>;
    using cell_pair_slice_collection = std::vector<cell_pair_slice>;

    cell_pair_slice_collection collection;
  };

} // namespace sand::ecal

UFW_DECLARE_MANAGED_DATA(sand::ecal::cell_pair_slices_container);

// For dictionaries
UFW_DECLARE_UNMANAGED_DATA(sand::ecal::cell_pair_slices_container::cell_pair_slice);
