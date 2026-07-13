#ifndef SAND_TRUTH_FILLER_DETAILS_HPP
#define SAND_TRUTH_FILLER_DETAILS_HPP

#include <vector>

namespace caf {
  class SRTrueInteraction;
  class SRTrueParticle;
  class TrueParticleID;
} // namespace caf

class StdHep;
class GRooTrackerEvent;

class EDEPTree;
class EDEPTrajectory;

namespace sand::common::filler_details {

  /// One interaction's contiguous slice of the flat `Primaries` vector.
  struct InteractionRange {
    std::size_t first_primary_index;
    std::size_t primary_count;
  };

  /// All top-level (primary) edep-sim trajectories, in interaction order.
  using Primaries = std::vector<EDEPTrajectory>;

  /// Result of build_true_particle_tree(): one interaction's primaries and secondaries,
  /// fully cross-linked via caf::TrueParticleID, plus post-FSI primary particle counts.
  struct TrueParticleTree {
    std::vector<::caf::SRTrueParticle> prim;
    std::vector<::caf::SRTrueParticle> sec;
    int nproton{};
    int nneutron{};
    int npip{};
    int npim{};
    int npi0{};
  };

  /// Groups `primaries` into contiguous per-interaction ranges using EDEPTrajectory::GetInteractionNumber().
  [[nodiscard]] std::vector<InteractionRange> make_interaction_ranges(Primaries const& primaries);

  /// @return true if TDatabasePDG classifies `pdg` as a Lepton.
  [[nodiscard]] bool is_lepton_pdg(int pdg);

  [[nodiscard]] bool is_darkneutrino_pdg(int pdg);

  [[nodiscard]] inline bool is_bindino_pdg(int pdg) { return pdg == 2000000101; }

  /// @return index (in `stdhep`) of the neutrino's single daughter, i.e. the final-state lepton.
  [[nodiscard]] int find_final_lepton(StdHep const& stdhep);

  /// Builds the vertex-level SRTrueInteraction from a GENIE gRooTracker event.
  [[nodiscard]] ::caf::SRTrueInteraction true_interaction_from_genie(GRooTrackerEvent const& event,
                                                                     StdHep const& stdhep);

  /// Builds one SRTrueParticle from StdHep entry `index`. Used for `prefsi` only.
  [[nodiscard]] ::caf::SRTrueParticle true_particle_from_genie(std::size_t index, StdHep const& stdhep,
                                                               long int ixn_id);

  /// Pre-FSI hadrons (status `kIStHadronInTheNucleus`, excluding bindino) for one interaction.
  [[nodiscard]] std::vector<::caf::SRTrueParticle> make_prefsi_particles(StdHep const& stdhep, long int ixn_id);

  /// Builds one SRTrueParticle from an edep-sim trajectory. Used for both `prim` and `sec`.
  /// @param ancestor_id  the root primary's TrueParticleID, propagated unchanged down the whole subtree.
  [[nodiscard]] ::caf::SRTrueParticle true_particle_from_edep(EDEPTrajectory const& traj, long int ixn_id,
                                                              ::caf::TrueParticleID const& ancestor_id);

  /// @return number of nodes in the subtree rooted at `t`, including `t` itself.
  [[nodiscard]] int subtree_node_count(EDEPTrajectory const& t);

  /// Builds the full, flattened event tree (primaries + all edep-sim descendants at any
  /// depth) for one interaction, cross-linked via caf::TrueParticleID (ancestor_id/parentID/daughtersID).
  /// @param sr_ixn          index of this interaction in the SRTruthBranch (-> TrueParticleID::ixn);
  ///                        distinct from `interaction_id`.
  /// @param interaction_id  edep-sim vertex ID (-> SRTrueParticle::interaction_id).
  [[nodiscard]] TrueParticleTree build_true_particle_tree(Primaries const& primaries, std::size_t first_idx,
                                                          std::size_t count, int sr_ixn, long int interaction_id);

} // namespace sand::common::filler_details

#endif
