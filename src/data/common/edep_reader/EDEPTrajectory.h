#pragma once

#include <edep_reader/EDEPHit.h>
#include <edep_reader/EDEPTrajectoryPoint.h>

/**
 * @class EDEPTrajectory
 * @brief Represents a trajectory of a particle through a detector.
 *
 * This class encapsulates information about a particle trajectory, including its initial momentum,
 * hits along its path, children trajectories, and various utility functions to query trajectory properties.
 */
class EDEPTrajectory {
 public:
  // Constructors
  EDEPTrajectory() : p0_(0, 0, 0, 0), parent_trajectory_(nullptr), id_(-1), parent_id_(-99), pdg_code_(0) {};

  EDEPTrajectory(const TG4Trajectory& trajectory)
    : p0_(trajectory.GetInitialMomentum()),
      parent_trajectory_(nullptr),
      id_(trajectory.GetTrackId()),
      parent_id_(trajectory.GetParentId()),
      pdg_code_(trajectory.GetPDGCode()) {};

  EDEPTrajectory(const TG4Trajectory& trajectory,
                 const std::map<int, std::map<component, std::vector<EDEPHit>>>& hit_map,
                 const TG4PrimaryVertexContainer& primaries);

  EDEPTrajectory(const EDEPTrajectory& trj);
  EDEPTrajectory(EDEPTrajectory&& trj);
  /**
   * @brief Destructor for EDEPTrajectory.
   */
  ~EDEPTrajectory() {};

  // Operators
  /**
   * @brief Equality operator for EDEPTrajectory.
   * @param trj The EDEPTrajectory object to compare.
   * @return True if the trajectories are equal, false otherwise.
   */
  bool operator== (const EDEPTrajectory& trj);

  /**
   * @brief Assignment operator for EDEPTrajectory.
   * @param trj The EDEPTrajectory object to assign.
   * @return Reference to the assigned EDEPTrajectory object.
   */
  EDEPTrajectory& operator= (const EDEPTrajectory& trj);

  /**
   * @brief Move assignment operator for EDEPTrajectory.
   * @param trj The EDEPTrajectory object to move.
   * @return Reference to the moved EDEPTrajectory object.
   */
  EDEPTrajectory& operator= (EDEPTrajectory&& trj);

  // Getters

  /**
   * @brief Get a pointer to this trajectory.
   * @return Pointer to this trajectory.
   */
  EDEPTrajectory* Get() { return this; };

  /**
   * @brief Get a const pointer to this trajectory.
   * @return Const pointer to this trajectory.
   */
  const EDEPTrajectory* Get() const { return this; };

  /**
   * @brief Get the parent trajectory of this trajectory.
   * @return Pointer to the parent trajectory.
   */
  EDEPTrajectory* GetParent() const { return parent_trajectory_; };

  /**
   * @brief Get the ID of this trajectory.
   * @return The ID of this trajectory.
   */
  int GetId() const { return id_; };

  /**
   * @brief Get the depth of this trajectory.
   * @return The depth of this trajectory.
   */
  int GetDepth() const { return depth_; };

  /**
   * @brief Get the number of the interaction that generated this trajectory.
   * @return The number of the interaction.
   */
  int GetInteractionNumber() const { return interaction_number_; };

  /**
   * @brief Get the reaction of the interaction that generated this trajectory.
   * @return The string corresponding to the reaction.
   */
  std::string GetReaction() const { return reaction_; };

  /**
   * @brief Get the parent ID of this trajectory.
   * @return The parent ID of this trajectory.
   */
  int GetParentId() const { return parent_id_; };

  /**
   * @brief Get the PDG code of this trajectory.
   * @return The PDG code of this trajectory.
   */
  int GetPDGCode() const { return pdg_code_; };

  std::vector<EDEPTrajectoryPoint>& GetFirstPointsInDetector(component component_name);
  std::vector<EDEPTrajectoryPoint>& GetLastPointsInDetector(component component_name);

  /**
   * @brief Get the initial momentum of this trajectory.
   * @return The initial momentum of this trajectory.
   */
  ROOT::Math::XYZTVector GetInitialMomentum() const { return p0_; };

  /**
   * @brief Get the children trajectories of this trajectory.
   * @return Reference to the vector of children trajectories.
   */
  std::vector<EDEPTrajectory>& GetChildrenTrajectories() { return children_trajectories_; };

  /**
   * @brief Get the const children trajectories of this trajectory.
   * @return Const reference to the vector of children trajectories.
   */
  const std::vector<EDEPTrajectory>& GetChildrenTrajectories() const { return children_trajectories_; };

  /**
   * @brief Get the hit map associated with this trajectory.
   * @return Const reference to the hit map associated with this trajectory.
   */
  const EDEPHitsMap& GetHitMap() const { return hit_map_; };

  /**
   * @brief Get the trajectory points associated with this trajectory.
   * @return Const reference to the trajectory points associated with this trajectory.
   */
  const EDEPTrajectoryPoints& GetTrajectoryPoints() const { return trajectory_points_; };

  /**
   * @brief Get the trajectory points associated with this trajectory.
   * @return All trajectory points associated with this trajectory ordered by increasing times.
   */
  std::vector<EDEPTrajectoryPoint> GetTrajectoryPointsVect();

  // Setters

  /**
   * @brief Set the ID of this trajectory.
   * @param id The ID to set.
   */
  void SetId(int id) { id_ = id; };

  /**
   * @brief Set the depth of this trajectory.
   * @param depth The depth to set.
   */
  void SetDepth(int depth) { depth_ = depth; };

  /**
   * @brief Set the parent ID of this trajectory.
   * @param parent_id The parent ID to set.
   */
  void SetParentId(int parent_id) { parent_id_ = parent_id; };

  /**
   * @brief Set the parent trajectory of this trajectory.
   * @param parent_trajectory The parent trajectory to set.
   */
  void SetParent(EDEPTrajectory* parent_trajectory) { parent_trajectory_ = parent_trajectory; };

  // Other

  /**
   * @brief Add a child trajectory to this trajectory.
   * @param trajectory The child trajectory to add.
   */
  void AddChild(const EDEPTrajectory& trajectory) {
    children_trajectories_.push_back(trajectory);
    children_trajectories_.back().SetParent(this);
  };
  bool RemoveChildWithId(int child_id);

  void ComputeDepth();

  // Utilities
  bool HasHits() const { return !hit_map_.empty(); }
  bool HasHitWithId(int id) const;
  bool HasHitInDetector(component component_name) const;
  double GetDepositedEnergy(component component_name);
  bool HasHitBeforeTime(double start_time) const;
  bool HasHitAfterTime(double stop_time) const;
  bool HasHitWithEnergySmallerThan(double energy) const;
  bool HasHitWithEnergyLargerThan(double energy) const; 
  bool HasHitInEnergy(double min, double max) const;
  bool HasHitInTimeAndEnergy(double start_time, double stop_time, 
                             double min_energy, double max_energy) const;

  bool IsTrajectorySaturated() const;

  bool HasHitInTime(double start_time, double stop_time) const;
  bool HasHitWithIdInDetector(int id, component component_name) const;

  bool HasHitNearPoint(ROOT::Math::XYZVector point, double distance) const;
  bool HasHitNear4DPoint(ROOT::Math::XYZTVector point, double distance, double time) const;
  std::vector<EDEPHit>::const_iterator GetHitNear4DPoint(ROOT::Math::XYZTVector point, double distance, double time);

  std::string Print(std::string& full_out, int depth = 100, int current_depth = 0) const;

  bool IsEntering(component component_name) const;
  bool IsExiting(component component_name) const;

  template <typename Funct>
  bool HasHitWhere(Funct&& f) const {
    for (const auto& hits : hit_map_) {
      for (const auto& hit : hits.second) {
        if (f(hit)) {
          return true;
        }
      }
    }
    return false;
  }

  template <typename Funct>
  std::vector<EDEPHit>::const_iterator GetHitWhere(Funct&& f) {
    for (const auto& hits : hit_map_) {
      for (auto it = hits.second.begin(); it != hits.second.end(); ++it) {
        if (f(*it)) {
          return it;
        }
      }
    }
    return hit_map_.rbegin()->second.end();
  }

  bool Match(std::string volume, std::initializer_list<std::string> names) const;

  void CheckInNext(bool* in, bool* next, TG4TrajectoryPoint it, TG4TrajectoryPoint next_it);

  friend class EDEPTree;

 private:
  ROOT::Math::XYZTVector p0_;                                 ///< Initial momentum of the trajectory.
  EDEPHitsMap hit_map_;                               ///< Map of hits associated with the trajectory.
  EDEPTrajectoryPoints trajectory_points_;            ///< Trajectory points.
  std::vector<EDEPTrajectory> children_trajectories_; ///< Children trajectories.
  std::map<component, bool> exiting_map_;             ///< Map indicating whether the trajectory is exiting a component.
  std::map<component, bool> entering_map_; ///< Map indicating whether the trajectory is entering a component.
  EDEPTrajectoryPoints last_points_;       ///< Map of all the first points in each component.
  EDEPTrajectoryPoints first_points_;      ///< Map of all the last points in each component.
  EDEPTrajectory* parent_trajectory_;      ///< Pointer to the parent trajectory.
  int id_;                                 ///< ID of the trajectory.
  int parent_id_;                          ///< Parent ID of the trajectory.
  int pdg_code_;                           ///< PDG code of the trajectory.
  int depth_;                              ///< Depth of the trajectory.
  int interaction_number_ = -1;            ///< Number of the interaction that generated this trajectory.
  std::string reaction_   = "";            ///< String corresponding to the reaction that generated this trajectory.
};
