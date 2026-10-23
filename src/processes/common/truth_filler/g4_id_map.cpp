#include "g4_id_map.hpp"

#include <ufw/utils.hpp>

#include <algorithm>

namespace sand::common {

  namespace {
    void collect_ids(EDEPTrajectory const& trj, std::vector<int>& ids) {
      ids.push_back(trj.GetId());
      for (auto const& child : trj.GetChildrenTrajectories()) {
        collect_ids(child, ids);
      }
    }

    /// @return the position of `id` in the sorted `ids`, -1 if it isn't there.
    int rank_of(std::vector<int> const& ids, int id) {
      auto const it = std::lower_bound(ids.begin(), ids.end(), id);
      return (it != ids.end() && *it == id) ? static_cast<int>(it - ids.begin()) : -1;
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

  G4IdMap::G4IdMap(Primaries const& primaries, InteractionRange const& range) {
    for (std::size_t i{}; i != range.primary_count; ++i) {
      auto const& prim = primaries[range.first_primary_index + i];
      m_prim_ids.push_back(prim.GetId());
      for (auto const& child : prim.GetChildrenTrajectories()) {
        collect_ids(child, m_sec_ids);
      }
    }
    std::sort(m_prim_ids.begin(), m_prim_ids.end());
    std::sort(m_sec_ids.begin(), m_sec_ids.end());
    if (!m_prim_ids.empty() && m_prim_ids.back() - m_prim_ids.front() + 1 != static_cast<int>(m_prim_ids.size())) {
      UFW_ERROR("Primary track ids [{}, {}] are not contiguous for {} primaries", m_prim_ids.front(), m_prim_ids.back(),
                m_prim_ids.size());
    }
    if (!m_sec_ids.empty() && m_sec_ids.back() - m_sec_ids.front() + 1 != static_cast<int>(m_sec_ids.size())) {
      UFW_ERROR("Secondary track ids [{}, {}] are not contiguous for {} secondaries", m_sec_ids.front(),
                m_sec_ids.back(), m_sec_ids.size());
    }
  }

  int G4IdMap::to_local(int edep_id) const {
    if (edep_id < 0) {
      return edep_id;
    }
    if (auto const r = rank_of(m_prim_ids, edep_id); r >= 0) {
      return r;
    }
    if (auto const r = rank_of(m_sec_ids, edep_id); r >= 0) {
      return static_cast<int>(m_prim_ids.size()) + r;
    }
    UFW_ERROR("Track id {} does not belong to this interaction", edep_id);
  }

  int G4IdMap::to_edep(int local_id) const {
    if (local_id < 0) {
      return local_id;
    }
    auto const idx = static_cast<std::size_t>(local_id);
    if (idx < m_prim_ids.size()) {
      return m_prim_ids[idx];
    }
    if (idx - m_prim_ids.size() < m_sec_ids.size()) {
      return m_sec_ids[idx - m_prim_ids.size()];
    }
    UFW_ERROR("Local track id {} does not belong to this interaction", local_id);
  }

  std::vector<G4IdMap> g4_id_maps(Primaries const& primaries) {
    std::vector<G4IdMap> maps;
    for (auto const& range : make_interaction_ranges(primaries)) {
      maps.emplace_back(primaries, range);
    }
    return maps;
  }

} // namespace sand::common
