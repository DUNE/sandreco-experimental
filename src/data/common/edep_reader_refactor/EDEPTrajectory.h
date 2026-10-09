#pragma once

#include <edep_reader_refactor/EDEPHit.h>
#include <edep_reader_refactor/EDEPTrajectoryPoint.h>

#include <array>
#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace sand::edep_refactor {

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
   * @brief A particle trajectory through the detector, with its hits, points and children.
   *
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
        pdg_code_(trajectory.GetPDGCode()) {}

    EDEPTrajectory(const EDEPTrajectory&)            = default;
    EDEPTrajectory(EDEPTrajectory&&)                 = default;
    EDEPTrajectory& operator= (const EDEPTrajectory&) = default;
    EDEPTrajectory& operator= (EDEPTrajectory&&)      = default;
    ~EDEPTrajectory()                                = default;

    bool operator== (const EDEPTrajectory& trj) const;

    // Getters
    EDEPTrajectory* Get() { return this; }
    const EDEPTrajectory* Get() const { return this; }
    EDEPTrajectory* GetParent() const { return parent_trajectory_; }
    int GetId() const { return id_; }
    int GetDepth() const { return depth_; }
    int GetInteractionNumber() const { return interaction_number_; }
    const std::string& GetReaction() const { return reaction_; }
    int GetParentId() const { return parent_id_; }
    int GetPDGCode() const { return pdg_code_; }
    sand::vec_4d GetInitialMomentum() const { return p0_; }

    std::vector<EDEPTrajectoryPoint>& GetFirstPointsInDetector(sand::subdetector_t c) { return first_points_[c]; }
    const std::vector<EDEPTrajectoryPoint>& GetFirstPointsInDetector(sand::subdetector_t c) const {
      return first_points_[c];
    }
    std::vector<EDEPTrajectoryPoint>& GetLastPointsInDetector(sand::subdetector_t c) { return last_points_[c]; }
    const std::vector<EDEPTrajectoryPoint>& GetLastPointsInDetector(sand::subdetector_t c) const {
      return last_points_[c];
    }

    trajectory_range<EDEPTrajectory> GetChildrenTrajectories() { return trajectory_range<EDEPTrajectory>(children_); }
    trajectory_range<const EDEPTrajectory> GetChildrenTrajectories() const {
      return trajectory_range<const EDEPTrajectory>(children_);
    }

    const EDEPHitsMap& GetHitMap() const { return hit_map_; }
    const EDEPTrajectoryPoints& GetTrajectoryPoints() const { return trajectory_points_; }
    /// All the trajectory points ordered by increasing time.
    std::vector<EDEPTrajectoryPoint> GetTrajectoryPointsVect() const;

    // Hit queries
    bool HasHits() const { return !hit_map_.empty(); }
    bool HasHitWithId(int id) const { return FindHit(id) != nullptr; }
    const EDEPHit& GetHitWithId(int id) const;
    bool HasHitInDetector(sand::subdetector_t c) const { return hit_map_.find(c) != hit_map_.end(); }
    bool HasHitWithIdInDetector(int id, sand::subdetector_t c) const;
    /// Sum of the secondary deposits in @p c, 0 if there are no hits there.
    double GetDepositedEnergy(sand::subdetector_t c) const;
    bool HasHitBeforeTime(double t) const;
    bool HasHitAfterTime(double t) const;
    bool HasHitWithEnergySmallerThan(double e) const;
    bool HasHitWithEnergyLargerThan(double e) const;
    bool HasHitInEnergy(double min, double max) const;
    bool HasHitInTime(double start_time, double stop_time) const;
    bool HasHitInTimeAndEnergy(double start_time, double stop_time, double min_energy, double max_energy) const;
    bool HasHitNearPoint(sand::pos_3d point, double distance) const;
    bool HasHitNear4DPoint(sand::vec_4d point, double distance, double time) const;
    /// First hit near the 4D point, nullptr if none.
    const EDEPHit* GetHitNear4DPoint(sand::vec_4d point, double distance, double time) const;

    /// True if the trajectory reached the edep-sim limit on the number of stored points.
    bool IsTrajectorySaturated() const;
    bool IsEntering(sand::subdetector_t c) const { return entering_[c]; }
    bool IsExiting(sand::subdetector_t c) const { return exiting_[c]; }

    std::string Print(std::string& full_out, int depth = 100, int current_depth = 0) const;

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

    sand::vec_4d p0_{0, 0, 0, 0};
    EDEPHitsMap hit_map_;
    EDEPTrajectoryPoints trajectory_points_;
    std::vector<EDEPTrajectory*> children_;
    std::array<bool, sand::kNumComponents> exiting_{};
    std::array<bool, sand::kNumComponents> entering_{};
    mutable std::array<std::vector<EDEPTrajectoryPoint>, sand::kNumComponents> last_points_;
    mutable std::array<std::vector<EDEPTrajectoryPoint>, sand::kNumComponents> first_points_;
    EDEPTrajectory* parent_trajectory_ = nullptr;
    int id_                            = -1;
    int parent_id_                     = -99;
    int pdg_code_                      = 0;
    int depth_                         = -1;
    int interaction_number_            = -1;
    std::string reaction_;
  };

} // namespace sand::edep_refactor
