#ifndef SAND_TRACKER_FAST_RECO
#define SAND_TRACKER_FAST_RECO

#include <edep_reader/edep_reader.hpp>

#include <ufw/process.hpp>

#include <duneanaobj/StandardRecord/SRTrack.h>
#include <duneanaobj/StandardRecord/SRShower.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>

#include <vector>

namespace sand::tracker {

  using TrackerG4IDs = std::vector<int>;

  [[nodiscard]] TrackerG4IDs g4ids_from_edep(sand::edep_reader const& tree);

  [[nodiscard]] ::caf::SRTrack track_from_true(::caf::SRTrueParticle const& true_part, ::caf::TrueParticleID const& id,
                                               EDEPTrajectory const* trj);

  [[nodiscard]] ::caf::SRShower shower_from_true(::caf::SRTrueParticle const& true_part,
                                                 ::caf::TrueParticleID const& id);

  struct fast_reco : public ufw::process {
    fast_reco();
    void run() override;
  };

} // namespace sand::tracker

UFW_REGISTER_PROCESS(sand::tracker::fast_reco);

#endif
