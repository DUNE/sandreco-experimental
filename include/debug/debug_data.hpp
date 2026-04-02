#ifndef DEBUG_INFO_HPP
#define DEBUG_INFO_HPP

#include <string>
#include <vector>

#include <ufw/data.hpp>
#include <edep_reader/EDEPHit.h>

namespace sand::debug {

struct trajectory_debug {
    int pdg{};
    int trj_idx{};
    std::vector<EDEPHit> hits{};
};

struct interaction_debug {
    int interaction_idx{};
    std::vector<trajectory_debug> trajectories{};
};

struct debug_data : public ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
  std::vector<interaction_debug> edep_interactions{};

  void clear() {
    edep_interactions.clear();
  }
};

} // namespace sand::debug

UFW_REGISTER_TYPE_NAME(sand::debug::debug_data, "sand::debug::debug_data")
UFW_DECLARE_RTTI(sand::debug::debug_data)

#endif // DEBUG_INFO_HPP
