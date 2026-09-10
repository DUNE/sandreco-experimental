#pragma once

#include <vector>

#include <common/sand.h>

namespace sand::grain {

  // Placeholder class for vertex
  struct vertex : managed_data_base {
    std::vector<pos_3d> vertices;  // vector for spill
  };

} // namespace sand::grain

UFW_DECLARE_MANAGED_DATA(sand::grain::vertex)
