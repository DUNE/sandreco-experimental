#pragma once

#include <common/truth.h>
#include <grain/grain.h>


namespace sand::grain {

  struct total_energy : managed_data_base {
    using energy_list = std::vector<float>;
    energy_list energies;
  };

}

UFW_DECLARE_MANAGED_DATA(sand::grain::total_energy)

