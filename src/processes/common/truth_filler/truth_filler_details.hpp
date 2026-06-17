#ifndef SAND_TRUTH_FILLER_DETAILS_HPP
#define SAND_TRUTH_FILLER_DETAILS_HPP

#include "types.hpp"

#include <genie_reader/GenieWrapper.h>
#include <sand.h>

#include <duneanaobj/StandardRecord/SRTrueInteraction.h>

namespace sand::common::filler_details {

  [[nodiscard]] std::vector<InteractionRange> make_interaction_ranges(Primaries const& primaries);

  [[nodiscard]] bool is_lepton_pdg(int pdg);

  [[nodiscard]] inline bool is_darkneutrino_pdg(int pdg) {
    const int abs_pdg = std::abs(pdg);
    return abs_pdg == 2000030000;
  }

  [[nodiscard]] inline bool is_bindino_pdg(int pdg) { return pdg == 2000000101; }

  [[nodiscard]] int find_final_lepton(StdHep const& stdhep);

  [[nodiscard]] Kinematics calculate_kinematics(sand::mom_4d const& nu_p4, sand::mom_4d const& lep_p4);

  [[nodiscard]] ::caf::SRTrueInteraction true_interaction_from_genie(GRooTrackerEvent const& event,
                                                                     StdHep const& stdhep);

  [[nodiscard]] ::caf::SRTrueParticle true_particle_from_genie(std::size_t index, StdHep const& stdhep,
                                                               long int ixn_id);

  [[nodiscard]] std::vector<::caf::SRTrueParticle> make_prefsi_particles(StdHep const& stdhep, long int ixn_id);

} // namespace sand::common::filler_details

#endif
