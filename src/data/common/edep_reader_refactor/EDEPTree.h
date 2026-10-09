#pragma once

#include <edep_reader_refactor/EDEPTrajectory.h>

#include <unordered_map>
#include <vector>

class TGeoNode;
class TGeoNavigator;

namespace sand::edep_refactor {

  /**
   * @class EDEPTree
   * @brief Tree of the trajectories of an edep-sim event. The tree itself is the (id -1) root, its children are
   *        the primaries.
   *
   * Compared to the legacy tree, the trajectories live in one flat storage built once per event and are linked
   * by pointers, so building is linear in the number of trajectories and nothing is copied. Iteration is a
   * depth-first, pre-order walk of all trajectories (the root excluded), as in the legacy tree. Lookups by
   * trajectory id and by hit id are O(1).
   *
   * The tree is read-only after InizializeFromEdep(): the legacy mutators (AddTrajectory, RemoveTrajectory,
   * MoveTrajectoryTo, ...) are not provided.
   */
  class EDEPTree : public EDEPTrajectory {
   public:
    using iterator       = indirect_iterator<EDEPTrajectory>;
    using const_iterator = indirect_iterator<const EDEPTrajectory>;

    EDEPTree() = default;
    // Trajectories point to the root and into the internal storage, so the tree cannot be copied or moved.
    EDEPTree(const EDEPTree&)            = delete;
    EDEPTree(EDEPTree&&)                 = delete;
    EDEPTree& operator= (const EDEPTree&) = delete;
    EDEPTree& operator= (EDEPTree&&)      = delete;

    iterator begin() { return iterator(order_.begin()); }
    iterator end() { return iterator(order_.end()); }
    const_iterator begin() const { return const_iterator(order_.begin()); }
    const_iterator end() const { return const_iterator(order_.end()); }
    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }
    /// Total number of trajectories in the tree.
    std::size_t size() const { return order_.size(); }

    /// Rebuilds the tree from @p edep_event, replacing the previous content.
    void InizializeFromEdep(const TG4Event& edep_event);

    bool HasTrajectory(int trj_id) const { return id_to_pos_.find(trj_id) != id_to_pos_.end(); }
    iterator GetTrajectory(int trj_id) { return at_pos(find_pos(trj_id)); }
    const_iterator GetTrajectory(int trj_id) const { return at_pos(find_pos(trj_id)); }
    /// Iterator to the parent of @p trj_id, end() if the trajectory is unknown or a primary.
    iterator GetParentOf(int trj_id);
    const_iterator GetParentOf(int trj_id) const;

    /// Iterator to the trajectory that produced hit @p id, end() if there is none.
    iterator GetTrajectoryWithHitId(int id) { return at_pos(hit_pos(id)); }
    const_iterator GetTrajectoryWithHitId(int id) const { return at_pos(hit_pos(id)); }
    iterator GetTrajectoryWithHitIdInDetector(int id, sand::subdetector_t c);
    const_iterator GetTrajectoryWithHitIdInDetector(int id, sand::subdetector_t c) const;
    /// The hit with global index @p id, nullptr if it does not belong to any trajectory.
    const EDEPHit* GetHit(int id) const {
      return (id >= 0 && static_cast<std::size_t>(id) < hit_ptr_.size()) ? hit_ptr_[id] : nullptr;
    }

    /// Copies the trajectories satisfying @p funct to @p out_it.
    template <typename OutputIterator, typename F>
    OutputIterator Filter(OutputIterator out_it, F&& funct) const {
      for (const auto& trj : *this) {
        if (funct(trj)) {
          *out_it = trj;
          ++out_it;
        }
      }
      return out_it;
    }

   private:
    /// Classification of a geometry volume, from its name.
    struct volume_class {
      sand::subdetector_t comp = sand::subdetector_t::OTHER; ///< component used for first/last points
      std::array<sand::subdetector_t, 2> route{};            ///< components receiving the trajectory points
      int n_route = 0;
      unsigned in_mask = 0; ///< bit i set if the volume is in component i
    };

    const volume_class& classify(const TGeoNode* node);
    void fill_points(EDEPTrajectory& trj, const TG4Trajectory& g4trj, TGeoNavigator& nav);
    void check_in_next(EDEPTrajectory& trj, const volume_class& in, const volume_class& next,
                       const EDEPTrajectoryPoint& pt, const TG4TrajectoryPoint& next_pt) const;

    int find_pos(int trj_id) const {
      auto it = id_to_pos_.find(trj_id);
      return it == id_to_pos_.end() ? -1 : it->second;
    }
    int hit_pos(int hit_id) const {
      return (hit_id >= 0 && static_cast<std::size_t>(hit_id) < hit_to_pos_.size()) ? hit_to_pos_[hit_id] : -1;
    }
    iterator at_pos(int pos) { return pos < 0 ? end() : begin() + pos; }
    const_iterator at_pos(int pos) const { return pos < 0 ? end() : begin() + pos; }

    std::vector<EDEPTrajectory> nodes_;   ///< storage, in event order; never reallocated while the tree is alive
    std::vector<EDEPTrajectory*> order_;  ///< depth-first pre-order view of nodes_
    std::unordered_map<int, int> id_to_pos_;  ///< trajectory id -> position in order_
    std::vector<int> hit_to_pos_;             ///< hit id -> position in order_ of its trajectory, -1 if none
    std::vector<const EDEPHit*> hit_ptr_;     ///< hit id -> hit
    std::unordered_map<const TGeoNode*, volume_class> volume_cache_; ///< survives across events
  };

} // namespace sand::edep_refactor
