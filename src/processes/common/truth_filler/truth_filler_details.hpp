#ifndef SAND_TRUTH_FILLER_DETAILS_HPP
#define SAND_TRUTH_FILLER_DETAILS_HPP

#include "sand.h"
#include <duneanaobj/StandardRecord/SRTrueInteraction.h>

#include <genie_reader/GenieWrapper.h>

namespace sand::mctruth {

  [[nodiscard]] bool is_lepton_pdg(int pdg);

  [[nodiscard]] int find_final_lepton(StdHep const& stdhep);

  struct Kinematics {
    float Q2{};
    float q0{};
    float modq{};
    float W{};
    float bjorkenX{};
    float inelasticity{};
  };

  [[nodiscard]] Kinematics calculate_kinematics(sand::mom_4d const& nu_p4, sand::mom_4d const& lep_p4);

  [[nodiscard]] ::caf::SRTrueInteraction true_interaction_from_genie(GRooTrackerEvent const& event,
                                                                     StdHep const& stdhep);

} // namespace sand::mctruth

#endif
