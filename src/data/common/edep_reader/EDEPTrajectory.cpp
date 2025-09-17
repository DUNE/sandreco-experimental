#include "EDEPTrajectory.h"

#include <ufw/context.hpp>

#include <root_tgeomanager/root_tgeomanager.hpp>

#include <math.h>

/**
 * @brief Retrieves the geometric node corresponding to a trajectory point.
 * @param tpoint The trajectory point.
 * @param geo The TGeoManager object representing the geometry.
 * @return Pointer to the geometric node.
 */
TGeoNode* GetNode(const TG4TrajectoryPoint& tpoint) {

  auto& tgm        = ufw::context::current()->instance<sand::root_tgeomanager>();
  
  TGeoNode* node = nullptr;

  TLorentzVector position = tpoint.GetPosition();
  TVector3       mom = tpoint.GetMomentum();

  node = tgm.navigator()->FindNode(position.X(), position.Y(), position.Z());

  tgm.navigator()->SetCurrentDirection(mom.X(), mom.Y(), mom.Z());

  // Notice: this is a fix for cases of P = 0.
  //         Particle not moving means FindNextBoundary will not converge
  //         Is this edepsim?
  if (abs(mom.X()) == 0 && abs(mom.Y()) == 0 && abs(mom.Z()) == 0) {
    return node;
  }
  tgm.navigator()->FindNextBoundary(1000);

  if (tgm.navigator()->GetStep() < 1E-5) {
    tgm.navigator()->Step();
    node = tgm.navigator()->GetCurrentNode();
  }

  return node;
}

// Notice: this works (I think) but I don't like it.
/**
 * @brief Checks for transitions between detector components in a particle trajectory.
 *
 * This function examines the current and next trajectory points to determine
 * if a transition occurs from one detector component to another.
 * If a transition is detected, it updates the entering and exiting state
 * of the respective components and stores the associated trajectory points.
 *
 * The `in` and `next` arrays should reflect the following order of components:
 * - 0: GRAIN
 * - 1: STRAW
 * - 2: DRIFT
 * - 3: ECAL
 * - 4: MAGNET
 * - 5: WORLD
 *
 * @param in Pointer to a boolean array indicating the components the current
 *           trajectory point is inside.
 * @param next Pointer to a boolean array indicating the components the next
 *             trajectory point is inside.
 * @param it The current trajectory point being checked.
 * @param next_it The next trajectory point being checked.
 */
void EDEPTrajectory::CheckInNext(bool* in, bool* next, TG4TrajectoryPoint it, TG4TrajectoryPoint next_it) {
  for (int i = 0; i < 6; i++) {
    for (int j = 0; j < 6; j++) {
      // Notice: this should be fixed in the geometry.
      if ((components[i] == component::STRAW && components[j] == component::DRIFT)
          || (components[j] == component::STRAW && components[i] == component::DRIFT)) {
        continue;
      }
      if (i == j) {
        continue;
      }
      if (in[i] && next[j]) {
        last_points_[components[i]].push_back(it);
        first_points_[components[j]].push_back(next_it);

        exiting_map_[components[i]]  = true;
        entering_map_[components[j]] = true;

        return;
      }
    }
  }
  return;
}

/**
 * @brief Constructs an EDEPTrajectory object from a TG4Trajectory and hit information.
 *
 * This constructor converts a TG4Trajectory object into an EDEPTrajectory, categorizing
 * the trajectory points into different components based on geometry and associating
 * hits with the trajectory. It also handles entering and exiting of different volume
 * regions and maps hits to their respective components.
 *
 * @param trajectory The TG4Trajectory object to be converted.
 * @param hits A map of hit segment detectors, containing hits associated with different components.
 * @param primaries A container for primary vertex information to track interaction number and reaction type.
 */
EDEPTrajectory::EDEPTrajectory(const TG4Trajectory& trajectory,
                               const std::map<int, std::map<component, std::vector<EDEPHit>>>& hit_map,
                               const TG4PrimaryVertexContainer& primaries)
  : p0_(trajectory.GetInitialMomentum()),
    parent_trajectory_(nullptr),
    id_(trajectory.GetTrackId()),
    parent_id_(trajectory.GetParentId()),
    pdg_code_(trajectory.GetPDGCode()) {
  for (const auto& vertex : primaries) {
    auto primary_trj_it =
        std::find_if(vertex.Particles.begin(), vertex.Particles.end(),
                     [this](TG4PrimaryParticle primary_trj) { return primary_trj.GetTrackId() == this->id_; });
    if (primary_trj_it != vertex.Particles.end()) {
      interaction_number_ = vertex.GetInteractionNumber();
      reaction_           = vertex.GetReaction();
      break;
    }
  }

  for (auto comp : string_to_component) {
    last_points_[comp.second]  = {};
    first_points_[comp.second] = {};

    exiting_map_[comp.second]  = false;
    entering_map_[comp.second] = false;
  }

  for (auto it = trajectory.Points.begin(); it != trajectory.Points.end(); ++it) {
    auto next_it = std::next(it);

    auto current_node   = GetNode(*it);
    std::string current_volume;
    if(current_node != nullptr ){
          current_volume = current_node->GetName();
    } else {
      current_volume = "";
    }

    component comp;
    bool in_grain   = Match(current_volume, grain_names);
    if(in_grain) comp = component::GRAIN;

    bool in_ecal    = Match(current_volume, ecal_names);
    if(in_ecal) comp = component::ECAL;

    bool in_mag     = Match(current_volume, magnet_names);
    if(in_mag) comp = component::MAGNET;

    bool in_world   = Match(current_volume, world_names);
    if(in_world) comp = component::WORLD;

    bool in_stt     = Match(current_volume, stt_names);
    bool in_drift   = Match(current_volume, drift_names);

    if(in_stt )   comp = component::STRAW;
    if(in_drift)  comp = component::DRIFT;

    // Notice: This is useful to debug unincluded volume names
    // if (!(in_grain || in_stt || in_drift || in_ecal || in_mag || in_world)) {
    // continue;
    // std::cout << current_volume << "  " << component_to_string[comp] << std::endl;
    // }

    // Notice: this should simply be 'trajectory_points_[comp].push_back(*it);'
    //         but stt and drift share some names and only the last one will be added
    //         must be fixed in the geometry

    if (in_grain || in_ecal || in_mag || in_world) {
      trajectory_points_[comp].push_back(*it);
    }

    if (in_stt && in_drift) {
      trajectory_points_[component::STRAW].push_back(*it);
      trajectory_points_[component::DRIFT].push_back(*it);
    }
    if (!in_stt && in_drift) {
      trajectory_points_[component::DRIFT].push_back(*it);
    }
    if (in_stt && !in_drift) {
      trajectory_points_[component::STRAW].push_back(*it);
    }

    if(it == trajectory.Points.begin()){
      first_points_[comp].push_back(*it);
    }
    if(next_it == trajectory.Points.end()){
      last_points_[comp].push_back(*it);
      continue;
    }

    auto next_node   = GetNode(*next_it);
    std::string next_volume;
    if(next_node != nullptr){
        next_volume = next_node->GetName();
    } else {
      next_volume = "";
    }

    bool next_grain = Match(next_volume,    grain_names);
    bool next_stt   = Match(next_volume,    stt_names);
    bool next_drift = Match(next_volume,    drift_names);
    bool next_ecal  = Match(next_volume,    ecal_names);
    bool next_mag   = Match(next_volume,    magnet_names);
    bool next_world = Match(next_volume,    world_names);

    bool      in[6]   = {in_grain,   in_stt,   in_drift,   in_ecal,   in_mag,   in_world};
    bool      next[6] = {next_grain, next_stt, next_drift, next_ecal, next_mag, next_world};
    CheckInNext(in, next, *it, *next_it);

  }

  if (hit_map.find(id_) != hit_map.end()) {
    hit_map_ = hit_map.at(id_);
    for (auto& hits : hit_map_) {
      std::sort(hits.second.begin(), hits.second.end(),
                [](EDEPHit i, EDEPHit j) { return i.GetStart().T() < j.GetStart().T(); });
    }
  }
}

/**
 * @brief Copy constructor for the EDEPTrajectory class.
 * @param trj The EDEPTrajectory object to copy.
 */
EDEPTrajectory::EDEPTrajectory(const EDEPTrajectory& trj) { *this = trj; }

/**
 * @brief Copy assignment operator for the EDEPTrajectory class.
 * @param trj The EDEPTrajectory object to copy.
 * @return Reference to the copied EDEPTrajectory object.
 */
EDEPTrajectory& EDEPTrajectory::operator= (const EDEPTrajectory& trj) {
  this->id_                    = trj.id_;
  this->parent_id_             = trj.parent_id_;
  this->parent_trajectory_     = trj.parent_trajectory_;
  this->pdg_code_              = trj.pdg_code_;
  this->p0_                    = trj.p0_;
  this->depth_                 = trj.depth_;
  this->interaction_number_    = trj.interaction_number_;
  this->reaction_              = trj.reaction_;
  this->children_trajectories_ = trj.children_trajectories_;
  for (auto& t : this->children_trajectories_) {
    t.parent_trajectory_ = this;
  }
  this->hit_map_           = trj.hit_map_;
  this->entering_map_      = trj.entering_map_;
  this->exiting_map_       = trj.exiting_map_;
  this->last_points_       = trj.last_points_;
  this->first_points_      = trj.first_points_;
  this->trajectory_points_ = trj.trajectory_points_;
  return *this;
}

bool EDEPTrajectory::operator== (const EDEPTrajectory& trj) {
  return (this->id_ == trj.id_ && this->parent_id_ == trj.parent_id_
          && this->parent_trajectory_ == trj.parent_trajectory_ && this->pdg_code_ == trj.pdg_code_
          && this->p0_ == trj.p0_ && this->depth_ == trj.depth_ && this->interaction_number_ == trj.interaction_number_
          && this->reaction_ == trj.reaction_ &&
          // this->children_trajectories_ == trj.children_trajectories_ &&
          // this->hit_map_ == trj.hit_map_ &&
          this->entering_map_ == trj.entering_map_ && this->exiting_map_ == trj.exiting_map_
          // this->last_points_ == trj.last_points_ &&
          // this -> first_points_ == trj.first_points_
          // this->trajectory_points_ == trj.trajectory_points_
  );
}

/**
 * @brief Copy assignment operator for the EDEPTrajectory class.
 * @param trj The EDEPTrajectory object to copy.
 * @return Reference to the copied EDEPTrajectory object.
 */
EDEPTrajectory::EDEPTrajectory(EDEPTrajectory&& trj) { *this = std::move(trj); }

/**
 * @brief Move assignment operator for the EDEPTrajectory class.
 * @param trj The EDEPTrajectory object to move.
 * @return Reference to the moved EDEPTrajectory object.
 */
EDEPTrajectory& EDEPTrajectory::operator= (EDEPTrajectory&& trj) {
  this->id_                 = trj.id_;
  this->parent_id_          = trj.parent_id_;
  this->parent_trajectory_  = trj.parent_trajectory_;
  this->pdg_code_           = trj.pdg_code_;
  this->p0_                 = trj.p0_;
  this->depth_              = trj.depth_;
  this->interaction_number_ = trj.interaction_number_;
  this->reaction_           = trj.reaction_;
  std::swap(this->children_trajectories_, trj.children_trajectories_);
  std::swap(this->hit_map_, trj.hit_map_);
  std::swap(this->entering_map_, trj.entering_map_);
  std::swap(this->exiting_map_, trj.exiting_map_);
  std::swap(this->first_points_, trj.first_points_);
  std::swap(this->last_points_, trj.last_points_);
  std::swap(this->trajectory_points_, trj.trajectory_points_);
  for (auto& t : this->children_trajectories_) {
    t.parent_trajectory_ = this;
  }

  trj.parent_trajectory_ = nullptr;
  return *this;
}

/**
 * @brief Removes a child trajectory with the given ID.
 * @param child_id ID of the child trajectory to remove.
 * @return True if a child with the given ID was removed, otherwise false.
 */
bool EDEPTrajectory::RemoveChildWithId(int child_id) {
  auto end = std::remove_if(children_trajectories_.begin(), children_trajectories_.end(),
                            [child_id](const EDEPTrajectory& trj) { return trj.GetId() == child_id; });
  auto b   = children_trajectories_.end() != end;
  children_trajectories_.erase(end, children_trajectories_.end());

  return b;
}

/**
 * @brief Prints the trajectory information to stdout and stores it in a string.
 * @param full_out Reference to a string to store the trajectory information.
 * @param depth Maximum depth to print.
 * @param current_depth Current depth in the tree.
 * @return String containing the trajectory information.
 */
std::string EDEPTrajectory::Print(std::string& full_out, int depth, int current_depth) const {
  if (current_depth <= depth) {
    for (int i = 0; i < current_depth; i++) {
      if (i != current_depth - 1) {
        std::cout << "    ";
        full_out += "    ";
      } else {
        std::cout << "|-- ";
        full_out += "|-- ";
      }
    }

    std::cout << this->GetDepth() << " " << this->GetId() << " " << this->GetInteractionNumber() << " " << this << " "
              << this->GetPDGCode() << std::endl;
    if (this->GetDepth() == 0)
      std::cout << this->GetReaction() << std::endl;
    full_out += std::to_string(this->GetDepth()) + " " + std::to_string(this->GetId()) + " "
              + std::to_string(this->GetPDGCode()) + "\n";

    for (const auto& el : hit_map_) {
      for (int i = 0; i < current_depth; i++) {
        if (i != current_depth) {
          std::cout << "    ";
          full_out += "    ";
        } else {
          std::cout << "|-- ";
          full_out += "|-- ";
        }
      }

      std::cout << component_to_string[el.first] << " " << el.second.size() << "; ";
      full_out += component_to_string[el.first] + " " + std::to_string(el.second.size()) + "; ";
      if (hit_map_.size() > 0) {
        std::cout << std::endl;
        full_out += "\n";
      }
    }
    for (uint i = 0; i < this->children_trajectories_.size(); ++i) {
      this->children_trajectories_.at(i).Print(full_out, depth, current_depth + 1);
    }
  }

  return full_out;
}

/**
 * @brief Computes the depth of the trajectory in the tree.
 */
void EDEPTrajectory::ComputeDepth() {
  int depth       = -1;
  auto tmp_parent = parent_trajectory_;
  while (tmp_parent != nullptr) {
    depth++;
    tmp_parent = tmp_parent->GetParent();
  }
  SetDepth(depth);
}

/**
 * @brief Checks if the trajectory has hits in the specified detector component.
 * @param component_name The name of the detector component.
 * @return True if the trajectory has hits in the specified component, otherwise false.
 */
bool EDEPTrajectory::HasHitInDetector(component component_name) const {
  return hit_map_.find(component_name) != hit_map_.end();
}

/**
 * @brief Checks if the trajectory has hits before a specified time.
 * @param time The time threshold.
 * @return True if the trajectory has hits before the specified time, otherwise false.
 */
bool EDEPTrajectory::HasHitBeforeTime(double time) const {
  return HasHitWhere([time](const EDEPHit& hit) { return hit.GetStart().T() < time; });
}

/**
 * @brief Checks if the trajectory has hits after a specified time.
 * @param time The time threshold.
 * @return True if the trajectory has hits after the specified time, otherwise false.
 */
bool EDEPTrajectory::HasHitAfterTime(double time) const {
  return HasHitWhere([time](const EDEPHit& hit) { return hit.GetStart().T() > time; });
}

/**
 * @brief Checks if the trajectory has hits within a specified time range.
 * @param start_time The start of the time range.
 * @param stop_time The end of the time range.
 * @return True if the trajectory has hits within the specified time range, otherwise false.
 */
bool EDEPTrajectory::HasHitInTime(double start_time, double stop_time) const {
  return HasHitWhere([start_time, stop_time](const EDEPHit& hit) {
    return (hit.GetStart().T() > start_time && hit.GetStart().T() < stop_time);
  });
}

/**
 * @brief Checks if any hit is within a given distance from a point.
 *
 * This function determines if the midpoint of any hit is closer than the
 * specified distance to the given point.
 *
 * @param point The reference point as a TVector3.
 * @param distance The maximum distance to consider.
 * @return true if a hit is within the specified distance from the point, false otherwise.
 */
bool EDEPTrajectory::HasHitNearPoint(TVector3 point, double distance) const {
  return HasHitWhere([point, distance](const EDEPHit& hit) {
    TVector3 middle_hit_point = (hit.GetStart().Vect() + hit.GetStop().Vect()) * 0.5;
    double hit_point_distance = (middle_hit_point - point).Mag();
    return fabs(hit_point_distance) < distance;
  });
}

/**
 * @brief Checks if any hit is within a given 4D distance (space and time) from a point.
 *
 * This function checks if the midpoint of any hit is within the specified spatial
 * distance and time difference from a given 4D point.
 *
 * @param point The reference 4D point as a TLorentzVector (includes both space and time).
 * @param distance The maximum spatial distance to consider.
 * @param time The maximum time difference to consider.
 * @return true if a hit is within the specified space and time distance from the point, false otherwise.
 */
bool EDEPTrajectory::HasHitNear4DPoint(TLorentzVector point, double distance, double time) const {
  return HasHitWhere([&](const EDEPHit& hit) {
    TVector3 middle_hit_point             = (hit.GetStart().Vect() + hit.GetStop().Vect()) * 0.5;
    double middle_hit_point_time          = (hit.GetStart().T() + hit.GetStop().T()) * 0.5;
    double hit_point_space_distance       = (middle_hit_point - point.Vect()).Mag();
    double middle_hit_point_time_distance = (middle_hit_point_time - point.T());
    return (fabs(hit_point_space_distance) < distance) && (fabs(middle_hit_point_time_distance) < time);
  });
}

/**
 * @brief Returns an iterator to the hit near a given 4D point (space and time).
 *
 * This function searches for a hit whose midpoint is within the specified spatial
 * distance and time difference from the given 4D point. It returns an iterator
 * to the first hit that matches the criteria.
 *
 * @param point The reference 4D point as a TLorentzVector (includes space and time components).
 * @param distance The maximum allowed spatial distance.
 * @param time The maximum allowed time difference.
 * @return An iterator to the first hit that is near the specified 4D point, or the end iterator if no hit matches.
 */
std::vector<EDEPHit>::const_iterator EDEPTrajectory::GetHitNear4DPoint(TLorentzVector point, double distance,
                                                                       double time) {
  return GetHitWhere([&](const EDEPHit& hit) {
    TVector3 middle_hit_point             = (hit.GetStart().Vect() + hit.GetStop().Vect()) * 0.5;
    double middle_hit_point_time          = (hit.GetStart().T() + hit.GetStop().T()) * 0.5;
    double hit_point_space_distance       = (middle_hit_point - point.Vect()).Mag();
    double middle_hit_point_time_distance = (middle_hit_point_time - point.T());
    return (fabs(hit_point_space_distance) < distance) && (fabs(middle_hit_point_time_distance) < time);
  });
}

/**
 * @brief Checks if any hit in the trajectory has the specified ID.
 *
 * This function searches through all hits in the trajectory and returns true if
 * any hit has a matching ID.
 *
 * @param id The ID to search for.
 * @return true if a hit with the specified ID is found, false otherwise.
 */
bool EDEPTrajectory::HasHitWithId(int id) const {
  auto f = [id](const EDEPHit& hit) { return (id == hit.GetId()) ? true : false; };
  for (const auto& hits : hit_map_) {
    auto found_hit = std::find_if(hits.second.begin(), hits.second.end(), f);
    bool result    = (found_hit != hits.second.end());
    if (result)
      return true;
  }
  return false;
}

/**
 * @brief Checks if the trajectory has a hit with the specified ID in the specified detector component.
 * @param id The ID of the hit.
 * @param component_name The name of the detector component.
 * @return True if the trajectory has a hit with the specified ID in the specified component, otherwise false.
 */
bool EDEPTrajectory::HasHitWithIdInDetector(int id, component component_name) const {
  if (GetHitMap().find(component_name) == GetHitMap().end())
    return false;

  auto f         = [id](const EDEPHit& hit) { return (id == hit.GetId()) ? true : false; };
  auto found_hit = std::find_if(GetHitMap().at(component_name).begin(), GetHitMap().at(component_name).end(), f);
  bool result    = (found_hit != GetHitMap().at(component_name).end());
  return result;
}

/**
 * @brief Calculates the total deposited energy in the specified detector component.
 * @param component_name The name of the detector component.
 * @return The total deposited energy in the specified component.
 */
double EDEPTrajectory::GetDepositedEnergy(component component_name) {
  double deposited_energy = 0;
  for (const auto& hit : hit_map_[component_name]) {
    deposited_energy += hit.GetSecondaryDeposit();
  }
  return deposited_energy;
}

/**
 * @brief Checks if the trajectory is saturated (i.e., has reached the maximum number of points).
 * @return True if the trajectory is saturated, otherwise false.
 */
bool EDEPTrajectory::IsTrajectorySaturated() const { return (trajectory_points_.size() == 10000); }

/**
 * @brief Matches a given volume name against a list of names.
 * @param volume The volume name to match.
 * @param names An initializer list of names to match against.
 * @return True if the volume name matches any of the names in the list, otherwise false.
 */
bool EDEPTrajectory::Match(std::string volume, std::initializer_list<std::string> names) const {
  for (const auto& n : names) {
    if (volume.find(n) != std::string::npos) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Checks if the trajectory is entering the specified detector component.
 * @param component_name The name of the detector component.
 * @return True if the trajectory is entering the specified component, otherwise false.
 */
bool EDEPTrajectory::IsEntering(component component_name) const { return entering_map_.at(component_name); }

/**
 * @brief Checks if the trajectory is exiting the specified detector component.
 * @param component_name The name of the detector component.
 * @return True if the trajectory is exiting the specified component, otherwise false.
 */
bool EDEPTrajectory::IsExiting(component component_name) const { return exiting_map_.at(component_name); }

std::vector<EDEPTrajectoryPoint>& EDEPTrajectory::GetFirstPointsInDetector(component component_name) {
  return first_points_.at(component_name);
}

std::vector<EDEPTrajectoryPoint>& EDEPTrajectory::GetLastPointsInDetector(component component_name) {
  return last_points_.at(component_name);
}

std::vector<EDEPTrajectoryPoint> EDEPTrajectory::GetTrajectoryPointsVect() const {
  std::vector<EDEPTrajectoryPoint> points;
  for (auto& comp : trajectory_points_) {
    points.insert(points.end(), comp.second.begin(), comp.second.end());
  }

  std::sort(points.begin(), points.end(),
            [](const EDEPTrajectoryPoint& i, const EDEPTrajectoryPoint& j) { return i.GetPosition().T() < j.GetPosition().T(); });

  return points;
}
