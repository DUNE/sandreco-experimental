#include "g4_id_map.hpp"

#include <ufw/utils.hpp>

#include <algorithm>
#include <iterator>
#include <numeric>

namespace sand {

  namespace {

    /// Ids of all the descendants of `trj`, not `trj` itself.
    IdBounds descendant_ids(EDEPTrajectory const& trj) {
      auto const children = trj.GetChildrenTrajectories();
      return std::accumulate(children.begin(), children.end(), IdBounds{},
                             [](IdBounds const& acc, EDEPTrajectory const& c) {
                               return merge_id_bounds(acc, merge_id_bounds(id_bounds_of(c.GetId()), descendant_ids(c)));
                             });
    }

  } // namespace

  std::vector<InteractionRange> make_interaction_ranges(Primaries const& primaries) {
    std::vector<InteractionRange> output;

    for (auto it = primaries.begin(); it != primaries.end();) {
      auto group_end = std::find_if_not(it, primaries.end(), [ixn = it->GetInteractionNumber()](auto const& p) {
        return p.GetInteractionNumber() == ixn;
      });
      output.push_back({static_cast<std::size_t>(it - primaries.begin()), static_cast<std::size_t>(group_end - it)});
      it = group_end;
    }

    return output;
  }

  int G4IdMap::to_interaction_id(int spill_id) const {
    if (spill_id < 0) {
      return spill_id;
    }
    if (m_prim.contains(spill_id)) {
      return spill_id - m_prim.first;
    }
    if (m_sec.contains(spill_id)) {
      return m_prim.count + spill_id - m_sec.first;
    }
    UFW_ERROR("Track id {} does not belong to this interaction", spill_id);
  }

  int G4IdMap::to_spill_id(int interaction_id) const {
    if (interaction_id < 0) {
      return interaction_id;
    }
    if (interaction_id < m_prim.count) {
      return m_prim.first + interaction_id;
    }
    if (interaction_id - m_prim.count < m_sec.count) {
      return m_sec.first + interaction_id - m_prim.count;
    }
    UFW_ERROR("Interaction track id {} does not belong to this interaction", interaction_id);
  }

  G4IdMap make_g4_id_map(Primaries const& primaries, InteractionRange const& range) {
    UFW_ASSERT(range.primary_count > 0, "make_g4_id_map: an interaction needs at least one primary");

    auto const first = std::next(primaries.begin(), static_cast<std::ptrdiff_t>(range.first_primary_index));
    auto const last  = std::next(first, static_cast<std::ptrdiff_t>(range.primary_count));

    auto const prim_ids = std::accumulate(first, last, IdBounds{}, [](IdBounds const& acc, EDEPTrajectory const& p) {
      return merge_id_bounds(acc, id_bounds_of(p.GetId()));
    });

    auto const sec_ids = std::accumulate(first, last, IdBounds{}, [](IdBounds const& acc, EDEPTrajectory const& p) {
      return merge_id_bounds(acc, descendant_ids(p));
    });

    auto const prim = contiguous_id_range(prim_ids);
    auto const sec  = contiguous_id_range(sec_ids);

    if (!prim || !sec) {
      auto const& bad = prim ? sec_ids : prim_ids;
      UFW_ERROR("Interaction {}: {} track ids [{}, {}] are not contiguous ({} ids)", first->GetInteractionNumber(),
                prim ? "secondary" : "primary", bad.min, bad.max, bad.count);
    }
    return G4IdMap{*prim, *sec};
  }

  std::vector<G4IdMap> g4_id_maps(Primaries const& primaries) {
    auto const ranges = make_interaction_ranges(primaries);
    std::vector<G4IdMap> maps;
    maps.reserve(ranges.size());
    for (auto const& range : ranges) {
      maps.push_back(make_g4_id_map(primaries, range));
    }
    return maps;
  }

} // namespace sand
