#ifndef SANDRECO_FAKE_RECO_PARTICLES_HPP
#define SANDRECO_FAKE_RECO_PARTICLES_HPP

#include <genie_reader/genie_reader.hpp>

#include <duneanaobj/StandardRecord/SRRecoParticle.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>

namespace sand::fake_reco {

  /// Create SRTrueParticle from GENIE StdHep data (pre-FSI particles)
  inline ::caf::SRTrueParticle SRTrueParticle_from_genie(const std::size_t i, const StdHep& std_hep) {
    return {
        .pdg            = std_hep.Pdg_[i],
        .G4ID           = -1, // This function is used only for genie only particles
        .interaction_id = -1, // This will be filled by fake_reco loop
        .time           = {}, // Genie doesn't have time data
        .ancestor_id    = {}, // A particle not propagated by GEANT can't descend from a GEANT primary
        .p         = TLorentzVector{std_hep.P4_[i].Px(), std_hep.P4_[i].Py(), std_hep.P4_[i].Pz(), std_hep.P4_[i].E()},
        .start_pos = {} // Genie spatial coordinates aren't in any reference system
                        // All the other fields are for GEANT data
    };
  }

} // namespace sand::fake_reco

#endif // SANDRECO_FAKE_RECO_PARTICLES_HPP
