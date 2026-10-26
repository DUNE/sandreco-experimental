#ifndef SAND_COMMON_G4_ID_MAP_HPP
#define SAND_COMMON_G4_ID_MAP_HPP

#include <vector>

#include <edep_reader/EDEPTrajectory.h>

namespace sand::common {

  /// One interaction's contiguous slice of the flat `Primaries` vector.
  struct InteractionRange {
    std::size_t first_primary_index;
    std::size_t primary_count;
  };

  /// All top-level (primary) edep-sim trajectories, in interaction order.
  using Primaries = trajectory_range<const EDEPTrajectory>;

  /// Groups `primaries` into contiguous per-interaction ranges using EDEPTrajectory::GetInteractionNumber().
  [[nodiscard]] std::vector<InteractionRange> make_interaction_ranges(Primaries const& primaries);

  /// Spill-wide edep-sim track ids <-> per-interaction ids (primaries first, then secondaries), as in ND_CAFMaker.
  ///
  /// An interaction id is the rank of the spill id among the interaction's primaries (then secondaries), sorted.
  /// SRTrueParticle::G4ID, parent and daughters are interaction ids: to look them up in the EDEPTree, go through
  /// to_spill_id().
  class G4IdMap {
    std::vector<int> m_prim_ids; ///< spill-wide ids of the primaries, sorted
    std::vector<int> m_sec_ids;  ///< spill-wide ids of all their descendants, sorted
   public:
    G4IdMap(Primaries const& primaries, InteractionRange const& range);

    /// @return the per-interaction id of the spill-wide edep-sim `spill_id`; negative ids are returned unchanged.
    [[nodiscard]] int to_interaction_id(int spill_id) const;

    /// @return the spill-wide edep-sim id of the per-interaction `interaction_id`; negative ids are returned unchanged.
    [[nodiscard]] int to_spill_id(int interaction_id) const;
  };

  /// One G4IdMap per interaction of the spill, indexed like SRTruthBranch::nu.
  [[nodiscard]] std::vector<G4IdMap> g4_id_maps(Primaries const& primaries);

} // namespace sand::common

#endif
