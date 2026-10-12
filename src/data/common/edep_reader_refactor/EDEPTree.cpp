#include "EDEPTree.h"

#include <ufw/context.hpp>

#include <root_tgeomanager/root_tgeomanager.hpp>

#include <TGeoManager.h>
#include <TGeoNavigator.h>
#include <TGeoNode.h>

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sand::edep_refactor {

  namespace {

    using sd = sand::subdetector_t;

    /// The volume a trajectory point is in, or is about to enter if it sits on a boundary.
    TGeoNode* get_node(TGeoNavigator& nav, const TG4TrajectoryPoint& tpoint) {
      const auto& pos = tpoint.GetPosition();
      const auto& mom = tpoint.GetMomentum();

      TGeoNode* node = nav.FindNode(pos.X(), pos.Y(), pos.Z());

      // At rest: no direction, so no boundary to look for.
      if (mom.X() == 0 && mom.Y() == 0 && mom.Z() == 0) {
        return node;
      }

      nav.SetCurrentDirection(mom.X(), mom.Y(), mom.Z());
      nav.FindNextBoundary(1000);
      if (nav.GetStep() < 1E-5) { // on a boundary: take the volume being entered
        nav.Step();
        node = nav.GetCurrentNode();
      }
      return node;
    }

    constexpr unsigned bit(unsigned c) { return 1u << c; }

    /// Main subdetector of a volume: the highest-precedence match, as in the legacy code.
    sd main_comp(unsigned mask) {
      for (sd c : {sd::DRIFT, sd::STT, sd::WORLD, sd::MAGNET, sd::ECAL, sd::GRAIN}) {
        if (mask & bit(c)) {
          return c;
        }
      }
      return sd::OTHER;
    }

    /// STT and DRIFT share volume names: a volume matching only those two feeds both point lists.
    bool is_stt_and_drift(unsigned mask) { return mask == (bit(sd::STT) | bit(sd::DRIFT)); }

    /// STT and DRIFT overlap in the geometry, so a transition between them is not a real one.
    bool is_tracker(unsigned c) { return c == sd::STT || c == sd::DRIFT; }

  } // namespace

  // ---------------------------------------------------------------------------------------------
  // Geometry
  // ---------------------------------------------------------------------------------------------

  /// Bitmask of the subdetectors a volume belongs to, from its name. Cached: the geometry never changes.
  unsigned EDEPTree::classify(const TGeoNode* node) {
    if (auto it = volume_cache_.find(node); it != volume_cache_.end()) {
      return it->second;
    }

    const std::string_view name = node ? std::string_view(node->GetName()) : std::string_view();

    unsigned mask = 0;
    if (Match(name, sand::grain_names)) mask |= bit(sd::GRAIN);
    if (Match(name, sand::ecal_names)) mask |= bit(sd::ECAL);
    if (Match(name, sand::magnet_names)) mask |= bit(sd::MAGNET);
    if (Match(name, sand::world_names)) mask |= bit(sd::WORLD);
    if (Match(name, sand::stt_names)) mask |= bit(sd::STT);
    if (Match(name, sand::drift_names) || Match(name, sand::generic_drift_names)) mask |= bit(sd::DRIFT);
    if (mask == 0) mask = bit(sd::OTHER);

    return volume_cache_.emplace(node, mask).first->second;
  }

  /// Records the first subdetector change between two consecutive points, if any.
  void EDEPTree::check_in_next(EDEPTrajectory& trj, unsigned in, unsigned next, const TG4TrajectoryPoint& pt,
                               const TG4TrajectoryPoint& next_pt) const {
    for (unsigned i = sd::kSandBegin; i < sd::kLoopableEnd; i++) {
      if (!(in & bit(i))) {
        continue;
      }
      for (unsigned j = sd::kSandBegin; j < sd::kLoopableEnd; j++) {
        if (!(next & bit(j)) || i == j || (is_tracker(i) && is_tracker(j))) {
          continue;
        }
        trj.last_points_[i].emplace_back(pt);
        trj.first_points_[j].emplace_back(next_pt);
        trj.exiting_[i]  = true;
        trj.entering_[j] = true;
        return; // one transition per step
      }
    }
  }

  void EDEPTree::fill_points(EDEPTrajectory& trj, const TG4Trajectory& g4trj, TGeoNavigator& nav) {
    const auto& pts = g4trj.Points;
    if (pts.empty()) {
      return;
    }

    // 1. Where is each point? Each point is located exactly once.
    auto& vol = point_volumes_; // member scratch buffer: keeps its capacity between calls
    vol.clear();
    for (const auto& p : pts) {
      vol.push_back(classify(get_node(nav, p)));
    }

    // 2. Store each point in its subdetector.
    for (std::size_t k = 0; k < pts.size(); k++) {
      trj.trajectory_points_[main_comp(vol[k])].emplace_back(pts[k]);
      if (is_stt_and_drift(vol[k])) {
        trj.trajectory_points_[sd::STT].emplace_back(pts[k]);
      }
    }

    // 3. First point of the trajectory.
    trj.first_points_[main_comp(vol.front())].emplace_back(pts.front());

    // 4. Transitions between consecutive points.
    for (std::size_t k = 0; k + 1 < pts.size(); k++) {
      check_in_next(trj, vol[k], vol[k + 1], pts[k], pts[k + 1]);
    }

    // 5. Last point of the trajectory (after the transitions, so every list stays in time order).
    trj.last_points_[main_comp(vol.back())].emplace_back(pts.back());
  }

  // ---------------------------------------------------------------------------------------------
  // Tree construction
  // ---------------------------------------------------------------------------------------------

  void EDEPTree::InizializeFromEdep(const TG4Event& edep_event) {
    // Reset. Containers keep their capacity, so a steady-state event allocates little.
    nodes_.clear();
    order_.clear();
    id_to_pos_.clear();
    hit_to_pos_.clear();
    hit_ptr_.clear();
    children_.clear();

    const std::size_t n_trj = edep_event.Trajectories.size();
    nodes_.reserve(n_trj); // addresses must stay stable from here on
    std::unordered_map<int, int> id_to_node;
    id_to_node.reserve(n_trj);

    // 1. Trajectories, in event order.
    for (const auto& g4trj : edep_event.Trajectories) {
      id_to_node.emplace(g4trj.GetTrackId(), static_cast<int>(nodes_.size()));
      nodes_.emplace_back(g4trj);
    }

    // 2. Interaction number and reaction of the primaries.
    {
      std::unordered_map<int, const TG4PrimaryVertex*> primary_vertex;
      for (const auto& vertex : edep_event.Primaries) {
        for (const auto& particle : vertex.Particles) {
          primary_vertex.emplace(particle.GetTrackId(), &vertex); // first vertex wins
        }
      }
      for (auto& trj : nodes_) {
        if (auto it = primary_vertex.find(trj.id_); it != primary_vertex.end()) {
          trj.interaction_number_ = it->second->GetInteractionNumber();
          trj.reaction_           = it->second->GetReaction();
        }
      }
    }

    // 3. Hits, straight into their trajectory. The hit index is global over all detectors.
    std::size_t n_hits = 0;
    {
      int ind = 0;
      for (const auto& [det_name, hits] : edep_event.SegmentDetectors) {
        auto comp_it    = sand::string_to_component.find(det_name);
        const auto comp = comp_it == sand::string_to_component.end() ? sd::OTHER : comp_it->second;
        for (const auto& hit : hits) {
          if (!hit.Contrib.empty()) {
            if (auto node = id_to_node.find(hit.Contrib[0]); node != id_to_node.end()) {
              nodes_[node->second].hit_map_[comp].emplace_back(hit, ind);
            }
          }
          ind++;
        }
      }
      n_hits = static_cast<std::size_t>(ind);
    }

    const auto by_time = [](const EDEPHit& a, const EDEPHit& b) { return a.GetStart().T() < b.GetStart().T(); };
    for (auto& trj : nodes_) {
      for (auto& [comp, hits] : trj.hit_map_) {
        if (!std::is_sorted(hits.begin(), hits.end(), by_time)) {
          std::stable_sort(hits.begin(), hits.end(), by_time);
        }
      }
    }

    // 4. Relations. A trajectory whose parent was not saved is attached to the root.
    for (auto& trj : nodes_) {
      EDEPTrajectory* parent = this;
      if (trj.parent_id_ != -1 && trj.parent_id_ != trj.id_) {
        if (auto p = id_to_node.find(trj.parent_id_); p != id_to_node.end()) {
          parent = &nodes_[p->second];
        } else {
          UFW_WARN("Trajectory {} has parent {}, which is not in the event: attaching it to the root", trj.id_,
                   trj.parent_id_);
        }
      }
      trj.parent_trajectory_ = parent;
      parent->children_.push_back(&trj);
    }

    // 5. Depth-first pre-order, depth and id lookup.
    order_.reserve(n_trj);
    id_to_pos_.reserve(n_trj);
    {
      std::vector<EDEPTrajectory*> stack(children_.rbegin(), children_.rend());
      depth_ = -1;
      while (!stack.empty()) {
        EDEPTrajectory* t = stack.back();
        stack.pop_back();
        t->depth_ = t->parent_trajectory_->depth_ + 1;
        id_to_pos_.emplace(t->id_, static_cast<int>(order_.size()));
        order_.push_back(t);
        stack.insert(stack.end(), t->children_.rbegin(), t->children_.rend());
      }
    }
    if (order_.size() != n_trj) {
      UFW_WARN("Only {} of {} trajectories are reachable from the root: the parent relations contain a cycle",
               order_.size(), n_trj);
    }

    // 6. Hit lookup tables.
    hit_to_pos_.assign(n_hits, -1);
    hit_ptr_.assign(n_hits, nullptr);
    for (std::size_t pos = 0; pos < order_.size(); pos++) {
      for (const auto& [comp, hits] : order_[pos]->hit_map_) {
        for (const auto& hit : hits) {
          hit_to_pos_[hit.GetId()] = static_cast<int>(pos);
          hit_ptr_[hit.GetId()]    = &hit;
        }
      }
    }

    // 7. Trajectory points: the expensive part, needs the geometry.
    auto& tgm = ufw::context::current()->instance<sand::root_tgeomanager>();
    auto nav  = tgm.navigator();
    for (std::size_t i = 0; i < n_trj; i++) {
      fill_points(nodes_[i], edep_event.Trajectories[i], *nav);
    }
  }

  // ---------------------------------------------------------------------------------------------
  // Queries
  // ---------------------------------------------------------------------------------------------

  EDEPTree::iterator EDEPTree::GetParentOf(int trj_id) {
    const int pos = find_pos(trj_id);
    if (pos < 0 || order_[pos]->parent_trajectory_ == this) {
      return end();
    }
    return at_pos(find_pos(order_[pos]->parent_trajectory_->id_));
  }

  EDEPTree::const_iterator EDEPTree::GetParentOf(int trj_id) const {
    const int pos = find_pos(trj_id);
    if (pos < 0 || order_[pos]->parent_trajectory_ == this) {
      return end();
    }
    return at_pos(find_pos(order_[pos]->parent_trajectory_->id_));
  }

  EDEPTree::iterator EDEPTree::GetTrajectoryWithHitIdInDetector(int id, sand::subdetector_t c) {
    const int pos = hit_pos(id);
    return (pos >= 0 && order_[pos]->HasHitWithIdInDetector(id, c)) ? at_pos(pos) : end();
  }

  EDEPTree::const_iterator EDEPTree::GetTrajectoryWithHitIdInDetector(int id, sand::subdetector_t c) const {
    const int pos = hit_pos(id);
    return (pos >= 0 && order_[pos]->HasHitWithIdInDetector(id, c)) ? at_pos(pos) : end();
  }

} // namespace sand::edep_refactor
