#pragma once

#include <vector>
#include <cstdint>

#include <ufw/data.hpp>
#include <common/truth.h>
#include <common/sand.h>

namespace sand::tracker {

  struct s_particle_infos : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {

    struct s_particle_info {
      int charge;
      double mass;
      int pdg_code;
      int id;

      pos_3d pos;
      mom_3d mom;
    };

    using s_particle_info_collection = std::vector<s_particle_info>;
    s_particle_info_collection particle_info_collection;
  };

}

UFW_DECLARE_MANAGED_DATA(sand::tracker::s_particle_infos)
