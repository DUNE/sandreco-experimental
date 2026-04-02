#include "debug_filler.hpp"


namespace sand {

  sand::debug::trajectory_debug from_edep(const EDEPTrajectory& traj, const ::caf::SRTrueParticle& prim) {
    sand::debug::trajectory_debug trj{};
    trj.pdg      = prim.pdg;
    trj.trj_idx  = prim.G4ID;

    const auto& hit_map = traj.GetHitMap();
    const auto& it = hit_map.find(component::DRIFT);
    if (it != hit_map.end()) {
      trj.hits = it->second;
    }
    return trj;
  }

} // namespace sand