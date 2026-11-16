#ifndef SAND_G4_ID_MAP_HPP
#define SAND_G4_ID_MAP_HPP

#include "id_range.hpp"

#include <edep_reader/EDEPTrajectory.h>

#include <vector>

namespace sand {

  /// One interaction's contiguous slice of `Primaries`.
  struct InteractionRange {
    std::size_t first_primary_index{};
    std::size_t primary_count{};
  };

  /// All top-level (primary) edep-sim trajectories of a spill, in interaction order.
  using Primaries = trajectory_range<const EDEPTrajectory>;

  /// Groups `primaries` into contiguous per-interaction slices, using EDEPTrajectory::GetInteractionNumber().
  ///
  /// Precondition: the primaries of one interaction are adjacent in `primaries`, as edep-sim writes them. An
  /// interaction split in two blocks would give two slices, without any error.
  std::vector<InteractionRange> make_interaction_ranges(Primaries const& primaries);

  /// Spill-wide edep-sim track ids <-> per-interaction ids (primaries first, then secondaries), as in ND_CAFMaker.
  ///
  /// An interaction id is the position of the spill id among the interaction's primaries, then among its
  /// secondaries (all the descendants of the primaries), both in increasing order. SRTrueParticle::G4ID, parent and
  /// daughters are interaction ids: to look them up in the EDEPTree, go through to_spill_id().
  class G4IdMap {
    IdRange m_prim; ///< spill-wide ids of the interaction's primaries
    IdRange m_sec;  ///< spill-wide ids of all their descendants

   public:
    constexpr G4IdMap(IdRange prim, IdRange sec) noexcept : m_prim{prim}, m_sec{sec} {}

    /// @return the per-interaction id of the spill-wide `spill_id`; negative ids are returned unchanged.
    /// UFW_ERROR if `spill_id` does not belong to this interaction.
    int to_interaction_id(int spill_id) const;

    /// @return the spill-wide id of the per-interaction `interaction_id`; negative ids are returned unchanged.
    /// UFW_ERROR if `interaction_id` is past the last secondary of this interaction.
    int to_spill_id(int interaction_id) const;
  };

  /// Builds the map of one interaction by walking its primaries and all their descendants.
  ///
  /// `range` must hold at least one primary. UFW_ERROR if the primary or the secondary ids are not contiguous.
  G4IdMap make_g4_id_map(Primaries const& primaries, InteractionRange const& range);

  /// One G4IdMap per interaction of the spill, indexed like SRTruthBranch::nu.
  std::vector<G4IdMap> g4_id_maps(Primaries const& primaries);

} // namespace sand

#endif
