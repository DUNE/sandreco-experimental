#ifndef SANDRECO_FAKE_RECO_DEBUG_FILLER_HPP
#define SANDRECO_FAKE_RECO_DEBUG_FILLER_HPP

/// @file debug_filler.hpp
/// @brief Filler to populate output files with lower-level information useful for debug analysis.

#include <edep_reader/edep_reader.hpp>
#include <edep_reader/EDEPTrajectory.h>
#include <edep_reader/EDEPHit.h>
#include <edep_reader/EDEPTree.h>
#include <genie_reader/GenieWrapper.h>

#include <debug/debug_data.hpp>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>
#include <vector>

namespace sand {

    /// @brief Fill debug_data with EDEPHit information from the EDEPTree / edep_reader.
    /// @param reader The EDepSim reader containing the event data.
    /// @return A debug_data struct populated with EDEPHit information.
    std::vector<EDEPHit> from_edep(const EDEPTrajectory& traj);


} // namespace sand

#endif // SANDRECO_FAKE_RECO_DEBUG_FILLER_HPP
