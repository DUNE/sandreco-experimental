#include "debug_filler.hpp"


namespace sand {

  std::vector<EDEPHit> from_edep(const EDEPTrajectory& traj) {
      const auto& hit_map = traj.GetHitMap();
      const auto& it = hit_map.find(component::DRIFT);
      if (it != hit_map.end())
          return it->second;
      return {};
  }

} // namespace sand