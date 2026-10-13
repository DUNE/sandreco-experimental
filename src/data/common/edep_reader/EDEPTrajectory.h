#pragma once

#include <edep_reader/EDEPHit.h>
#include <edep_reader/EDEPTrajectoryPoint.h>

#include <array>
#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

class EDEPTrajectory;

  /**
   * Random access iterator over a vector of pointers, dereferencing to the pointee.
   * Lets trajectories keep their children as non-owning pointers while still exposing a range of trajectories.
   */
  template <typename T>
  class indirect_iterator {
    using base_type = std::vector<EDEPTrajectory*>::const_iterator;

   public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = std::remove_const_t<T>;
    using difference_type   = std::ptrdiff_t;
    using pointer           = T*;
    using reference         = T&;

    indirect_iterator() = default;
    explicit indirect_iterator(base_type b) : it_(b) {}
    template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    indirect_iterator(const indirect_iterator<U>& o) : it_(o.base()) {}

    base_type base() const { return it_; }

    reference operator* () const { return **it_; }
    pointer operator->() const { return *it_; }
    reference operator[] (difference_type n) const { return **(it_ + n); }

    indirect_iterator& operator++ () { ++it_; return *this; }
    indirect_iterator operator++ (int) { auto t = *this; ++it_; return t; }
    indirect_iterator& operator-- () { --it_; return *this; }
    indirect_iterator operator-- (int) { auto t = *this; --it_; return t; }
    indirect_iterator& operator+= (difference_type n) { it_ += n; return *this; }
    indirect_iterator& operator-= (difference_type n) { it_ -= n; return *this; }
    friend indirect_iterator operator+ (indirect_iterator i, difference_type n) { return i += n; }
    friend indirect_iterator operator+ (difference_type n, indirect_iterator i) { return i += n; }
    friend indirect_iterator operator- (indirect_iterator i, difference_type n) { return i -= n; }
    friend difference_type operator- (const indirect_iterator& a, const indirect_iterator& b) { return a.it_ - b.it_; }
    friend bool operator== (const indirect_iterator& a, const indirect_iterator& b) { return a.it_ == b.it_; }
    friend bool operator!= (const indirect_iterator& a, const indirect_iterator& b) { return a.it_ != b.it_; }
    friend bool operator< (const indirect_iterator& a, const indirect_iterator& b) { return a.it_ < b.it_; }
    friend bool operator> (const indirect_iterator& a, const indirect_iterator& b) { return a.it_ > b.it_; }
    friend bool operator<= (const indirect_iterator& a, const indirect_iterator& b) { return a.it_ <= b.it_; }
    friend bool operator>= (const indirect_iterator& a, const indirect_iterator& b) { return a.it_ >= b.it_; }

   private:
    base_type it_{};
  };

  /// Non-owning view over a vector of trajectory pointers, usable as a range of trajectory references.
  template <typename T>
  class trajectory_range {
   public:
    using iterator       = indirect_iterator<T>;
    using const_iterator = indirect_iterator<T>;
    using value_type     = std::remove_const_t<T>;

    explicit trajectory_range(const std::vector<EDEPTrajectory*>& v) : v_(&v) {}

    iterator begin() const { return iterator(v_->begin()); }
    iterator end() const { return iterator(v_->end()); }
    std::size_t size() const { return v_->size(); }
    bool empty() const { return v_->empty(); }
    T& operator[] (std::size_t i) const { return *(*v_)[i]; }
    T& at(std::size_t i) const { return *v_->at(i); }
    T& front() const { return *v_->front(); }
    T& back() const { return *v_->back(); }

   private:
    const std::vector<EDEPTrajectory*>* v_;
  };

/**
 * @class EDEPTrajectory
 * @brief Represents a trajectory of a particle through a detector.
 *
 * This class encapsulates information about a particle trajectory, including its initial momentum,
 * hits along its path, children trajectories, and various utility functions to query trajectory properties.
 * Trajectories are owned by an EDEPTree, which also fills them. Parent and children are non-owning pointers
 * into the tree's storage, so a trajectory copied out of its tree is only a snapshot: its relations still point
 * into the original tree and are valid only as long as the tree is not rebuilt.
 */
class EDEPTrajectory {
 public:
    EDEPTrajectory() = default;

  explicit EDEPTrajectory(const TG4Trajectory& trajectory)
    : p0_(trajectory.GetInitialMomentum()),
      id_(trajectory.GetTrackId()),
      parent_id_(trajectory.GetParentId()),
      pdg_code_(trajectory.GetPDGCode()) {};

  EDEPTrajectory(const EDEPTrajectory&)            = default;
  EDEPTrajectory(EDEPTrajectory&&)                 = default;
  
  /**
   * @brief Destructor for EDEPTrajectory.
   */
  ~EDEPTrajectory()                                = default;

  // Operators
  /**
   * @brief Equality operator for EDEPTrajectory.
   * @param trj The EDEPTrajectory object to compare.
   * @return True if the trajectories are equal, false otherwise.
   */
  bool operator== (const EDEPTrajectory& trj) const;

  /**
   * @brief Assignment operator for EDEPTrajectory.
   * @param trj The EDEPTrajectory object to assign.
   * @return Reference to the assigned EDEPTrajectory object.
   */
  EDEPTrajectory& operator= (const EDEPTrajectory&) = default;

  /**
   * @brief Move assignment operator for EDEPTrajectory.
   * @param trj The EDEPTrajectory object to move.
   * @return Reference to the moved EDEPTrajectory object.
   */
  EDEPTrajectory& operator= (EDEPTrajectory&&)      = default;

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

  std::vector<EDEPTrajectoryPoint>& GetFirstPointsInDetector(sand::subdetector_t c) { return first_points_[c]; }
  const std::vector<EDEPTrajectoryPoint>& GetFirstPointsInDetector(sand::subdetector_t c) const {
    return first_points_[c];
  }
  std::vector<EDEPTrajectoryPoint>& GetLastPointsInDetector(sand::subdetector_t c) { return last_points_[c]; }
  const std::vector<EDEPTrajectoryPoint>& GetLastPointsInDetector(sand::subdetector_t c) const {
    return last_points_[c];
  }

  /**
   * @brief Get the initial momentum of this trajectory.
   * @return The initial momentum of this trajectory.
   */
  sand::vec_4d GetInitialMomentum() const { return p0_; };
  trajectory_range<EDEPTrajectory> GetChildrenTrajectories() { return trajectory_range<EDEPTrajectory>(children_); };
 trajectory_range<const EDEPTrajectory> GetChildrenTrajectories() const { return trajectory_range<const EDEPTrajectory>(children_);}

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
  std::vector<EDEPTrajectoryPoint> GetTrajectoryPointsVect() const;

    // Hit queries
  bool HasHits() const { return !hit_map_.empty(); }
  bool HasHitWithId(int id) const { return FindHit(id) != nullptr; }
  const EDEPHit& GetHitWithId(int id) const;
  bool HasHitInDetector(sand::subdetector_t c) const { return hit_map_.find(c) != hit_map_.end(); }
  /// Sum of the secondary deposits in @p c, 0 if there are no hits there.
  double GetDepositedEnergy(sand::subdetector_t c) const;
  bool HasHitBeforeTime(double t) const;
  bool HasHitAfterTime(double t) const;
  bool HasHitWithEnergySmallerThan(double e) const;
  bool HasHitWithEnergyLargerThan(double e) const;
  bool HasHitInEnergy(double min, double max) const;
  bool HasHitInTimeAndEnergy(double start_time, double stop_time, double min_energy, double max_energy) const;
  /// True if the trajectory reached the edep-sim limit on the number of stored points.
  bool IsTrajectorySaturated() const;

  bool HasHitInTime(double start_time, double stop_time) const;
  bool HasHitWithIdInDetector(int id, sand::subdetector_t c) const;

  bool HasHitNearPoint(sand::pos_3d point, double distance) const;
  bool HasHitNear4DPoint(sand::vec_4d point, double distance, double time) const;
  /// First hit near the 4D point, nullptr if none.
  const EDEPHit* GetHitNear4DPoint(sand::vec_4d point, double distance, double time) const;


  
  std::string Print(std::string& full_out, int depth = 100, int current_depth = 0) const;

  bool IsEntering(sand::subdetector_t c) const { return entering_[c]; };
  bool IsExiting(sand::subdetector_t c) const { return exiting_[c]; };

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
  const EDEPHit* GetHitWhere(Funct&& f) const {
    for (const auto& hits : hit_map_) {
      for (const auto& hit : hits.second) {
        if (f(hit)) {
          return &hit;
        }
      }
    }
    return nullptr;
  }

  /// True if @p volume contains any of @p names as a substring.
  static bool Match(std::string_view volume, std::initializer_list<std::string> names);

  friend class EDEPTree;

  protected:
  const EDEPHit* FindHit(int id) const;

  static constexpr std::size_t kMaxPoints = 10000;

  sand::vec_4d p0_{0, 0, 0, 0};                       ///< Initial momentum of the trajectory.
  EDEPHitsMap hit_map_;                               ///< Map of hits associated with the trajectory.
  EDEPTrajectoryPoints trajectory_points_;            ///< Trajectory points.
  std::vector<EDEPTrajectory*> children_;             ///< Children trajectories.
  std::array<bool, sand::kNumComponents> exiting_{};  ///< Map indicating whether the trajectory is exiting a sand::subdetector_t.
  std::array<bool, sand::kNumComponents> entering_{}; ///< Map indicating whether the trajectory is entering a sand::subdetector_t.
  mutable std::array<std::vector<EDEPTrajectoryPoint>, sand::kNumComponents> last_points_;    ///< Map of all the first points in each sand::subdetector_t.
  mutable std::array<std::vector<EDEPTrajectoryPoint>, sand::kNumComponents> first_points_;   ///< Map of all the last points in each sand::subdetector_t.
  EDEPTrajectory* parent_trajectory_ = nullptr;        ///< Pointer to the parent trajectory.
  int id_                            = -1;             ///< ID of the trajectory.
  int parent_id_                     = -99;            ///< Parent ID of the trajectory.
  int pdg_code_                      = 0;              ///< PDG code of the trajectory.
  int depth_                         = -1;             ///< Depth of the trajectory.
  int interaction_number_            = -1;             ///< Number of the interaction that generated this trajectory.
  std::string reaction_;                               ///< String corresponding to the reaction that generated this trajectory.
};
