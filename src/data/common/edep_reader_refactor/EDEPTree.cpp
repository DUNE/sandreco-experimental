#include "EDEPTree.h"

#include <ufw/context.hpp>

#include <root_tgeomanager/root_tgeomanager.hpp>

#include <TGeoManager.h>
#include <TGeoNavigator.h>
#include <TGeoNode.h>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace sand::edep_refactor {

  namespace {

    /// Finds the geometry node a trajectory point is in (or is about to enter), as the legacy GetNode().
    TGeoNode* get_node(TGeoNavigator& nav, const TG4TrajectoryPoint& tpoint) {
      const auto& position = tpoint.GetPosition();
      const auto& mom      = tpoint.GetMomentum();

      TGeoNode* node = nav.FindNode(position.X(), position.Y(), position.Z());
      nav.SetCurrentDirection(mom.X(), mom.Y(), mom.Z());

      // A particle at rest means FindNextBoundary will not converge.
      if (mom.X() == 0 && mom.Y() == 0 && mom.Z() == 0) {
        return node;
      }
      nav.FindNextBoundary(1000);
      if (nav.GetStep() < 1E-5) {
        nav.Step();
        node = nav.GetCurrentNode();
      }
      return node;
    }

    constexpr unsigned bit(sand::subdetector_t c) { return 1u << static_cast<unsigned>(c); }

  } // namespace

  const EDEPTree::volume_class& EDEPTree::classify(const TGeoNode* node) {
    auto cached = volume_cache_.find(node);
    if (cached != volume_cache_.end()) {
      return cached->second;
    }

    const std::string_view name = node != nullptr ? std::string_view(node->GetName()) : std::string_view();

    const bool grain         = Match(name, sand::grain_names);
    const bool ecal          = Match(name, sand::ecal_names);
    const bool mag           = Match(name, sand::magnet_names);
    const bool world         = Match(name, sand::world_names);
    const bool stt           = Match(name, sand::stt_names);
    const bool drift         = Match(name, sand::drift_names);
    const bool generic_drift = Match(name, sand::generic_drift_names);
    const bool any_drift     = drift || generic_drift;

    volume_class vc;
    // Same precedence as the legacy code: later matches override earlier ones.
    if (grain) vc.comp = sand::subdetector_t::GRAIN;
    if (ecal) vc.comp = sand::subdetector_t::ECAL;
    if (mag) vc.comp = sand::subdetector_t::MAGNET;
    if (world) vc.comp = sand::subdetector_t::WORLD;
    if (stt) vc.comp = sand::subdetector_t::STT;
    if (any_drift) vc.comp = sand::subdetector_t::DRIFT;

    // stt and drift share some names, so a volume can feed both.
    if (grain || ecal || mag || world) {
      vc.route[vc.n_route++] = vc.comp;
    } else if (stt && any_drift) {
      vc.route[vc.n_route++] = sand::subdetector_t::STT;
      vc.route[vc.n_route++] = sand::subdetector_t::DRIFT;
    } else if (any_drift) {
      vc.route[vc.n_route++] = sand::subdetector_t::DRIFT;
    } else if (stt) {
      vc.route[vc.n_route++] = sand::subdetector_t::STT;
    } else {
      vc.route[vc.n_route++] = sand::subdetector_t::OTHER;
    }

    const bool other = !(grain || stt || any_drift || ecal || mag || world);
    if (grain) vc.in_mask |= bit(sand::subdetector_t::GRAIN);
    if (stt) vc.in_mask |= bit(sand::subdetector_t::STT);
    if (any_drift) vc.in_mask |= bit(sand::subdetector_t::DRIFT);
    if (ecal) vc.in_mask |= bit(sand::subdetector_t::ECAL);
    if (mag) vc.in_mask |= bit(sand::subdetector_t::MAGNET);
    if (world) vc.in_mask |= bit(sand::subdetector_t::WORLD);
    if (other) vc.in_mask |= bit(sand::subdetector_t::OTHER);

    return volume_cache_.emplace(node, vc).first->second;
  }

  void EDEPTree::check_in_next(EDEPTrajectory& trj, const volume_class& in, const volume_class& next,
                               const EDEPTrajectoryPoint& pt, const TG4TrajectoryPoint& next_pt) const {
    for (unsigned i = sand::subdetector_t::kSandBegin; i < sand::subdetector_t::kLoopableEnd; i++) {
      if (!(in.in_mask & (1u << i))) {
        continue;
      }
      for (unsigned j = sand::subdetector_t::kSandBegin; j < sand::subdetector_t::kLoopableEnd; j++) {
        if (i == j || !(next.in_mask & (1u << j))) {
          continue;
        }
        // stt and drift overlap in the geometry, a transition between them is not a real one.
        if ((i == sand::subdetector_t::STT && j == sand::subdetector_t::DRIFT)
            || (j == sand::subdetector_t::STT && i == sand::subdetector_t::DRIFT)) {
          continue;
        }
        trj.last_points_[i].push_back(pt);
        trj.first_points_[j].emplace_back(next_pt);
        trj.exiting_[i]  = true;
        trj.entering_[j] = true;
        return;
      }
    }
  }

  void EDEPTree::fill_points(EDEPTrajectory& trj, const TG4Trajectory& g4trj, TGeoNavigator& nav) {
    const auto& pts = g4trj.Points;
    if (pts.empty()) {
      return;
    }

    // Each point is classified once: the "next" volume of a point is the "current" one of the following.
    const volume_class* cur = &classify(get_node(nav, pts.front()));
    for (auto it = pts.begin(); it != pts.end(); ++it) {
      const EDEPTrajectoryPoint pt(*it);

      for (int r = 0; r < cur->n_route; r++) {
        trj.trajectory_points_[cur->route[r]].push_back(pt);
      }
      if (it == pts.begin()) {
        trj.first_points_[cur->comp].push_back(pt);
      }

      auto next_it = std::next(it);
      if (next_it == pts.end()) {
        trj.last_points_[cur->comp].push_back(pt);
        break;
      }

      const volume_class* nxt = &classify(get_node(nav, *next_it));
      check_in_next(trj, *cur, *nxt, pt, *next_it);
      cur = nxt;
    }
  }

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

    // 2. Interaction number and reaction of the primaries: one pass instead of a search per trajectory.
    {
      std::unordered_map<int, const TG4PrimaryVertex*> primary_vertex;
      for (const auto& vertex : edep_event.Primaries) {
        for (const auto& particle : vertex.Particles) {
          primary_vertex.emplace(particle.GetTrackId(), &vertex); // first vertex wins
        }
      }
      for (auto& trj : nodes_) {
        auto it = primary_vertex.find(trj.id_);
        if (it != primary_vertex.end()) {
          trj.interaction_number_ = it->second->GetInteractionNumber();
          trj.reaction_           = it->second->GetReaction();
        }
      }
    }

    // 3. Hits, straight into their trajectory. The hit index is global over all detectors.
    std::size_t n_hits = 0;
    for (const auto& det : edep_event.SegmentDetectors) {
      n_hits += det.second.size();
    }
    std::vector<int> hit_node(n_hits, -1);
    {
      int ind = 0;
      for (const auto& det : edep_event.SegmentDetectors) {
        auto comp_it = sand::string_to_component.find(det.first);
        const auto comp =
            comp_it == sand::string_to_component.end() ? sand::subdetector_t::OTHER : comp_it->second;
        for (const auto& hit : det.second) {
          if (!hit.Contrib.empty()) {
            auto node = id_to_node.find(hit.Contrib[0]);
            if (node != id_to_node.end()) {
              nodes_[node->second].hit_map_[comp].emplace_back(hit, ind);
              hit_node[ind] = node->second;
            }
          }
          ind++;
        }
      }
    }
    for (auto& trj : nodes_) {
      for (auto& hits : trj.hit_map_) {
        auto by_time = [](const EDEPHit& a, const EDEPHit& b) { return a.GetStart().T() < b.GetStart().T(); };
        if (!std::is_sorted(hits.second.begin(), hits.second.end(), by_time)) {
          std::stable_sort(hits.second.begin(), hits.second.end(), by_time);
        }
      }
    }

    // 4. Relations. A trajectory whose parent was not saved is attached to the root.
    for (auto& trj : nodes_) {
      EDEPTrajectory* parent = this;
      if (trj.parent_id_ != -1 && trj.parent_id_ != trj.id_) {
        auto p = id_to_node.find(trj.parent_id_);
        if (p != id_to_node.end()) {
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
      for (const auto& hits : order_[pos]->hit_map_) {
        for (const auto& hit : hits.second) {
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

  EDEPTree::iterator EDEPTree::GetParentOf(int trj_id) {
    int pos = find_pos(trj_id);
    if (pos < 0 || order_[pos]->parent_trajectory_ == this) {
      return end();
    }
    return at_pos(find_pos(order_[pos]->parent_trajectory_->id_));
  }

  EDEPTree::const_iterator EDEPTree::GetParentOf(int trj_id) const {
    int pos = find_pos(trj_id);
    if (pos < 0 || order_[pos]->parent_trajectory_ == this) {
      return end();
    }
    return at_pos(find_pos(order_[pos]->parent_trajectory_->id_));
  }

  EDEPTree::iterator EDEPTree::GetTrajectoryWithHitIdInDetector(int id, sand::subdetector_t c) {
    int pos = hit_pos(id);
    return (pos >= 0 && order_[pos]->HasHitWithIdInDetector(id, c)) ? at_pos(pos) : end();
  }

  EDEPTree::const_iterator EDEPTree::GetTrajectoryWithHitIdInDetector(int id, sand::subdetector_t c) const {
    int pos = hit_pos(id);
    return (pos >= 0 && order_[pos]->HasHitWithIdInDetector(id, c)) ? at_pos(pos) : end();
  }

} // namespace sand::edep_refactor
