#pragma once

#include <grain/grain.h>

namespace sand::grain {

  struct voxels : managed_data_base {
    using voxels_list = std::vector<voxel_array<float>>;
    voxels_list voxels;
  };

} // namespace sand::grain

UFW_DECLARE_MANAGED_DATA(sand::grain::voxels)

UFW_DECLARE_UNMANAGED_DATA(sand::grain::voxel_array<float>)
