#ifndef SAND_TRUTH_FILLER_DETAILS_HPP
#define SAND_TRUTH_FILLER_DETAILS_HPP

#include <genie_reader/GenieWrapper.h>
#include <sand.h>

#include <duneanaobj/StandardRecord/SRTrueInteraction.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>

#include <vector>

class EDEPTree;
class EDEPTrajectory;

namespace sand::common::filler_details {

  struct InteractionRange {
    std::size_t first_primary_index;
    std::size_t primary_count;
  };

  using Primaries = std::vector<EDEPTrajectory>;

  using AncestorIds = std::vector<::caf::TrueParticleID>;

  struct PrimariesResult {
    std::vector<::caf::SRTrueParticle> particles;
    AncestorIds ancestor_ids;
    int nproton{};
    int nneutron{};
    int npip{};
    int npim{};
    int npi0{};
  };

  [[nodiscard]] std::vector<InteractionRange> make_interaction_ranges(Primaries const& primaries);

  [[nodiscard]] bool is_lepton_pdg(int pdg);

  [[nodiscard]] inline bool is_darkneutrino_pdg(int pdg) {
    const int abs_pdg = std::abs(pdg);
    return abs_pdg == 2000030000;
  }

  [[nodiscard]] inline bool is_bindino_pdg(int pdg) { return pdg == 2000000101; }

  [[nodiscard]] int find_final_lepton(StdHep const& stdhep);

  [[nodiscard]] ::caf::SRTrueInteraction true_interaction_from_genie(GRooTrackerEvent const& event,
                                                                     StdHep const& stdhep);

  [[nodiscard]] ::caf::SRTrueParticle true_particle_from_genie(std::size_t index, StdHep const& stdhep,
                                                               long int ixn_id);

  [[nodiscard]] std::vector<::caf::SRTrueParticle> make_prefsi_particles(StdHep const& stdhep, long int ixn_id);

  [[nodiscard]] ::caf::SRTrueParticle true_particle_from_edep(EDEPTrajectory const& traj, long int ixn_id,
                                                              ::caf::TrueParticleID const& ancestor_id);

  [[nodiscard]] PrimariesResult make_primaries(Primaries const& primaries, std::size_t first_idx, std::size_t count,
                                               long int ixn_id);

  [[nodiscard]] std::vector<::caf::SRTrueParticle> make_secondaries(EDEPTree const& edep_tree,
                                                                    Primaries const& primaries, std::size_t first_idx,
                                                                    std::size_t count, AncestorIds const& ancestor_ids,
                                                                    long int ixn_id);

} // namespace sand::common::filler_details

#endif
