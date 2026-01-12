#include "EDEPTree.h"

// Iterators

/**
 * @brief Constructor for iterator class.
 * @param parent_trj Pointer to the parent trajectory.
 * @param child_it Iterator pointing to a child trajectory.
 */
EDEPTree::iterator::iterator(value_type* parent_trj, child_iterator child_it) {
  this->current_it_ = child_it;
  this->parent_trj_ = parent_trj;
}

/**
 * @brief Prefix increment operator for the iterator class.
 * @details Increments the iterator to the next trajectory in the tree.
 * @return Reference to the updated iterator.
 */
EDEPTree::iterator& EDEPTree::iterator::operator++ () {
  if (!current_it_->GetChildrenTrajectories().empty()) {
    parent_trj_ = &(*current_it_);
    current_it_ = current_it_->GetChildrenTrajectories().begin();

  } else {
    ++current_it_;
    while (parent_trj_ && (current_it_ == parent_trj_->GetChildrenTrajectories().end())) {
      auto grand_parent = parent_trj_->GetParent();
      if (!grand_parent) {
        current_it_ = parent_trj_->GetChildrenTrajectories().end();
        parent_trj_ = nullptr;
        break;
      }

      auto b = grand_parent->GetChildrenTrajectories().begin();

      // syntax for doing parent_trj_ + 1
      ptrdiff_t diff = parent_trj_ - &(*b); // granted by std::vector

      parent_trj_ = grand_parent;
      current_it_ = b + diff + 1;
    }
  }

  return *this;
}

/**
 * @brief Constructor for const_iterator class.
 * @param parent_trj Pointer to the parent trajectory.
 * @param child_it Iterator pointing to a child trajectory.
 */
EDEPTree::const_iterator::const_iterator(value_type* parent_trj, const_child_iterator child_it) {
  this->current_it_ = child_it;
  this->parent_trj_ = parent_trj;
}

/**
 * @brief Prefix increment operator for the const_iterator class.
 * @details Increments the const_iterator to the next trajectory in the tree.
 * @return Reference to the updated const_iterator.
 */
EDEPTree::const_iterator& EDEPTree::const_iterator::operator++ () {
  if (!current_it_->GetChildrenTrajectories().empty()) {
    parent_trj_ = &(*current_it_);
    current_it_ = current_it_->GetChildrenTrajectories().begin();

  } else {
    ++current_it_;
    while (parent_trj_ && (current_it_ == parent_trj_->GetChildrenTrajectories().end())) {
      auto grand_parent = parent_trj_->GetParent();
      if (!grand_parent) {
        current_it_ = parent_trj_->GetChildrenTrajectories().end();
        parent_trj_ = nullptr;
        break;
      }

      auto b = grand_parent->GetChildrenTrajectories().begin();

      // syntax for doing parent_trj_ + 1
      ptrdiff_t diff = parent_trj_ - &(*b); // granted by std::vector

      parent_trj_ = grand_parent;
      current_it_ = b + diff + 1;
    }
  }

  return *this;
}

/**
 * @brief Constructor for the EDEPTree class.
 * @details Initializes the tree with default values.
 */
EDEPTree::EDEPTree() {
  this->SetId(-1);
  this->SetParent(nullptr);
  this->SetDepth(-1);
}

/**
 * @brief Creates the tree structure from a vector of trajectories.
 * @param trajectories_vect Vector of trajectories.
 */
void EDEPTree::CreateTree(const std::vector<EDEPTrajectory>& trajectories_vect) {
  bool complete = true;
  while (complete) {
    complete = true;
    for (auto it = trajectories_vect.begin(); it != trajectories_vect.end(); it++) {
      if (!HasTrajectory((*it).GetId())) {
        complete  = false;
        int depth = 0;
        AddTrajectory((*it));
      }
    }
  }
  std::for_each(this->begin(), this->end(), [](EDEPTrajectory& trj) { trj.ComputeDepth(); });
}

/**
 * @brief Initializes the tree from a TG4Event
 * @param edep_event TG4Event object.
 */
void EDEPTree::InizializeFromEdep(const TG4Event& edep_event) {
  this->GetChildrenTrajectories().clear();
  std::map<int, std::map<component, std::vector<EDEPHit>>> hit_map;
  for (auto hmap : edep_event.SegmentDetectors) {
    for (uint i = 0; i < hmap.second.size(); i++) {
      auto h = hmap.second[i];
      if(string_to_component.find(hmap.first) == string_to_component.end()) {
 	hit_map[h.Contrib[0]][component::OTHER].push_back(EDEPHit(h, i));
      } else {
     	 hit_map[h.Contrib[0]][string_to_component[hmap.first]].push_back(EDEPHit(h, i));
      }
    }
  }
  std::vector<EDEPTrajectory> trajectories;
  for (auto trj : edep_event.Trajectories) {
    trajectories.push_back(EDEPTrajectory(trj, hit_map, edep_event.Primaries));
  }
  CreateTree(trajectories);
}

/**
 * @brief Initializes the tree from a vector of trajectories.
 * @param trajectories_vect Vector of trajectories.
 */
void EDEPTree::InizializeFromTrj(const std::vector<EDEPTrajectory>& trajectories_vect) {
  this->GetChildrenTrajectories().clear();
  CreateTree(trajectories_vect);
}

/**
 * @brief Adds a trajectory to the tree.
 * @param trajectory The trajectory to add.
 */
void EDEPTree::AddTrajectory(const EDEPTrajectory& trajectory) {
  int parent_id = trajectory.GetParentId();
  if (parent_id == -1) {
    this->AddChild(trajectory);
  } else {
    std::find_if(this->begin(), this->end(), [parent_id](const EDEPTrajectory& trj) {
      return parent_id == trj.GetId();
    })->AddChild(trajectory);
  }
}

/**
 * @brief Adds a trajectory to the tree at a specified position.
 * @param trajectory The trajectory to add.
 * @param it Iterator pointing to the position where the trajectory should be added.
 */
void EDEPTree::AddTrajectoryTo(const EDEPTrajectory& trajectory, iterator it) {
  int parent_id = trajectory.GetParentId();
  if (parent_id == -1) {
    this->AddChild(trajectory);
  } else {
    EDEPTree::iterator end_it = GetTrajectoryEnd(it);
    std::find_if(it, end_it, [parent_id](const EDEPTrajectory& trj) {
      return parent_id == trj.GetId();
    })->AddChild(trajectory);
  }
}

/**
 * @brief Removes a trajectory from the tree.
 * @param trj_id ID of the trajectory to remove.
 */
void EDEPTree::RemoveTrajectory(int trj_id) { GetParentOf(trj_id)->RemoveChildWithId(trj_id); }

/**
 * @brief Removes a trajectory from the tree at a specified position.
 * @param trj_id ID of the trajectory to remove.
 * @param it Iterator pointing to the position of the trajectory to remove.
 */
void EDEPTree::RemoveTrajectoryFrom(int trj_id, iterator it) { GetParentOf(trj_id, it)->RemoveChildWithId(trj_id); }

/**
 * @brief Moves a trajectory to a new parent trajectory.
 * @param id_to_move ID of the trajectory to move.
 * @param next_parent_id ID of the new parent trajectory.
 */
void EDEPTree::MoveTrajectoryTo(int id_to_move, int next_parent_id) {
  EDEPTrajectory trjToMove = *GetTrajectory(id_to_move);
  RemoveTrajectory(id_to_move);
  GetTrajectory(next_parent_id)->AddChild(trjToMove);
  std::for_each(this->begin(), this->end(), [](EDEPTrajectory& trj) { trj.ComputeDepth(); });
}

/**
 * @brief Checks if the tree contains a trajectory with the given ID.
 * @param trj_id ID of the trajectory to check.
 * @return True if the trajectory is found, otherwise false.
 */
bool EDEPTree::HasTrajectory(int trj_id) const {
  return std::find_if(this->begin(), this->end(), [trj_id](const EDEPTrajectory& trj) { return trj_id == trj.GetId(); })
      != this->end();
}

/**
 * @brief Checks if a trajectory is in a specified subtree.
 * @param trj_id ID of the trajectory to check.
 * @param it Iterator pointing to the subtree.
 * @return True if the trajectory is found in the subtree, otherwise false.
 */
bool EDEPTree::IsTrajectoryIn(int trj_id, iterator it) {
  EDEPTree::iterator end_it = GetTrajectoryEnd(it);
  EDEPTree::iterator found_it =
      std::find_if(it, end_it, [trj_id](const EDEPTrajectory& trj) { return trj_id == trj.GetId(); });
  return (found_it != end_it) ? true : false;
}

/**
 * @brief Checks if a trajectory is in a specified subtree.
 * @param trj_id ID of the trajectory to check.
 * @param it Iterator pointing to the subtree.
 * @return True if the trajectory is found in the subtree, otherwise false.
 */
bool EDEPTree::IsTrajectoryIn(int trj_id, const_iterator it) const {
  EDEPTree::const_iterator end_it = GetTrajectoryEnd(it);
  EDEPTree::const_iterator found_it =
      std::find_if(it, end_it, [trj_id](const EDEPTrajectory& trj) { return trj_id == trj.GetId(); });
  return (found_it != end_it) ? true : false;
}

/**
 * @brief Retrieves the parent trajectory of a trajectory with the given ID.
 * @param trj_id ID of the trajectory.
 * @return Iterator pointing to the parent trajectory.
 */
EDEPTree::iterator EDEPTree::GetParentOf(int trj_id) {
  auto f = [trj_id](const EDEPTrajectory& trj) {
    for (const auto& t : trj.GetChildrenTrajectories()) {
      if (trj_id == t.GetId())
        return true;
    };
    return false;
  };
  return std::find_if(this->begin(), this->end(), f);
}

/**
 * @brief Retrieves the parent trajectory of a trajectory with the given ID.
 * @param trj_id ID of the trajectory.
 * @return Iterator pointing to the parent trajectory.
 */
EDEPTree::const_iterator EDEPTree::GetParentOf(int trj_id) const {
  auto f = [trj_id](const EDEPTrajectory& trj) {
    for (const auto& t : trj.GetChildrenTrajectories()) {
      if (trj_id == t.GetId())
        return true;
    };
    return false;
  };
  return std::find_if(this->begin(), this->end(), f);
}

/**
 * @brief Retrieves the parent trajectory of a trajectory with the given ID within a specified subtree.
 * @param trj_id ID of the trajectory.
 * @param it Iterator pointing to the subtree.
 * @return Iterator pointing to the parent trajectory.
 */
EDEPTree::iterator EDEPTree::GetParentOf(int trj_id, iterator it) {
  EDEPTree::iterator end_it = GetTrajectoryEnd(it);
  auto f                    = [trj_id](const EDEPTrajectory& trj) {
    for (const auto& t : trj.GetChildrenTrajectories()) {
      if (trj_id == t.GetId())
        return true;
    };
    return false;
  };
  EDEPTree::iterator found_it = std::find_if(it, end_it, f);
  return (found_it != end_it) ? found_it : this->end();
}

/**
 * @brief Retrieves the parent trajectory of a trajectory with the given ID within a specified subtree.
 * @param trj_id ID of the trajectory.
 * @param it Iterator pointing to the subtree.
 * @return Iterator pointing to the parent trajectory.
 */
EDEPTree::const_iterator EDEPTree::GetParentOf(int tid, const_iterator it) const {
  EDEPTree::const_iterator end_it = GetTrajectoryEnd(it);
  auto f                          = [tid](const EDEPTrajectory& trj) {
    for (const auto& t : trj.GetChildrenTrajectories()) {
      if (tid == t.GetId())
        return true;
    };
    return false;
  };
  EDEPTree::const_iterator found_it = std::find_if(it, end_it, f);
  return (found_it != end_it) ? found_it : this->end();
}

/**
 * @brief Retrieves the iterator to the trajectory with the given ID within a specified subtree.
 * @param trj_id ID of the trajectory.
 * @param it Iterator pointing to the subtree.
 * @return Iterator pointing to the trajectory with the given ID.
 */
EDEPTree::iterator EDEPTree::GetTrajectoryFrom(int trj_id, iterator it) {
  EDEPTree::iterator end_it = GetTrajectoryEnd(it);
  EDEPTree::iterator found_it =
      std::find_if(it, end_it, [trj_id](const EDEPTrajectory& trj) { return trj_id == trj.GetId(); });
  return (found_it != end_it) ? found_it : end_it;
}

/**
 * @brief Retrieves the iterator to the trajectory with the given ID within a specified subtree.
 * @param trj_id ID of the trajectory.
 * @param it Iterator pointing to the subtree.
 * @return Iterator pointing to the trajectory with the given ID.
 */
EDEPTree::const_iterator EDEPTree::GetTrajectoryFrom(int trj_id, const_iterator it) const {
  EDEPTree::const_iterator end_it = GetTrajectoryEnd(it);
  EDEPTree::const_iterator found_it =
      std::find_if(it, end_it, [trj_id](const EDEPTrajectory& trj) { return trj_id == trj.GetId(); });
  return (found_it != end_it) ? found_it : this->end();
}

/**
 * @brief Returns an iterator to the trajectory containing a hit with the specified ID.
 *
 * This function searches through the tree for a trajectory that contains a hit
 * with the given ID and returns an iterator to the matching trajectory.
 *
 * @param id The ID of the hit to search for.
 * @return An iterator to the trajectory containing the hit, or the end iterator if no match is found.
 */
EDEPTree::iterator EDEPTree::GetTrajectoryWithHitId(int id) {
  EDEPTree::iterator found_it =
      std::find_if(this->begin(), this->end(), [id](const EDEPTrajectory& trj) { return trj.HasHitWithId(id); });
  return (found_it != this->end()) ? found_it : this->end();
}

/**
 * @brief Returns a const iterator to the trajectory containing a hit with the specified ID.
 *
 * This function searches through the tree for a trajectory that contains a hit
 * with the given ID and returns a const iterator to the matching trajectory.
 *
 * @param id The ID of the hit to search for.
 * @return A const iterator to the trajectory containing the hit, or the end iterator if no match is found.
 */
EDEPTree::const_iterator EDEPTree::GetTrajectoryWithHitId(int id) const {
  EDEPTree::const_iterator found_it =
      std::find_if(this->begin(), this->end(), [id](const EDEPTrajectory& trj) { return trj.HasHitWithId(id); });
  return (found_it != this->end()) ? found_it : this->end();
}

/**
 * @brief Retrieves the iterator to the trajectory containing a hit with the given ID in the specified detector
 * component.
 * @param id ID of the hit.
 * @param component_name Name of the detector component.
 * @return Iterator pointing to the trajectory containing the hit.
 */
EDEPTree::iterator EDEPTree::GetTrajectoryWithHitIdInDetector(int id, component component_name) {
  EDEPTree::iterator found_it =
      std::find_if(this->begin(), this->end(), [id, component_name](const EDEPTrajectory& trj) {
        return trj.HasHitWithIdInDetector(id, component_name);
      });
  return (found_it != this->end()) ? found_it : this->end();
}

/**
 * @brief Retrieves the iterator to the trajectory containing a hit with the given ID in the specified detector
 * component.
 * @param id ID of the hit.
 * @param component_name Name of the detector component.
 * @return Iterator pointing to the trajectory containing the hit.
 */
EDEPTree::const_iterator EDEPTree::GetTrajectoryWithHitIdInDetector(int id, component component_name) const {
  EDEPTree::const_iterator found_it =
      std::find_if(this->begin(), this->end(), [id, component_name](const EDEPTrajectory& trj) {
        return trj.HasHitWithIdInDetector(id, component_name);
      });
  return (found_it != this->end()) ? found_it : this->end();
}

/**
 * @brief Retrieves the iterator to the end of the subtree starting from the specified iterator position.
 * @param start Iterator pointing to the start of the subtree.
 * @return Iterator pointing to the end of the subtree.
 */
EDEPTree::iterator EDEPTree::GetTrajectoryEnd(iterator start) {
  EDEPTree::iterator end_it = start;
  if (!start->GetChildrenTrajectories().empty()) {
    end_it = ++iterator(start->Get(), --(start->GetChildrenTrajectories().end()));
  } else {
    end_it = ++start;
  }
  return end_it;
}

/**
 * @brief Retrieves the iterator to the end of the subtree starting from the specified iterator position.
 * @param start Iterator pointing to the start of the subtree.
 * @return Iterator pointing to the end of the subtree.
 */
EDEPTree::const_iterator EDEPTree::GetTrajectoryEnd(const_iterator start) const {
  EDEPTree::const_iterator end_it = start;
  if (!start->GetChildrenTrajectories().empty()) {
    end_it = ++const_iterator(start->Get(), --(start->GetChildrenTrajectories().end()));
  } else {
    end_it = ++start;
  }
  return end_it;
}
