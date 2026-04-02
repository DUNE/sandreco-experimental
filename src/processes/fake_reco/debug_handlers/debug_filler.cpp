#include "debug_filler.hpp"


namespace sand {

  sand::debug::debug_data from_edep(const EDEPTrajectory& traj) {
    sand::debug::debug_data info{};

    const auto& hit_map = traj.GetHitMap();
    const auto& it = hit_map.find(component::DRIFT);        
    // if(it == hit_map.end()) { return info; }
    const auto& hit_vec = it->second;
    info.hits.push_back(hit_vec); // da capire, perchè così diventa vector<vector<EDEPHit>> invece che vector<EDEPHit>
    

    return info;
  }

} // namespace sand