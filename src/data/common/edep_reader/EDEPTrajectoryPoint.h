#pragma once

#include <TG4Event.h>
#include <common/sand.h>

#include <map>
#include <vector>


/**
 * @class EDEPTrajectoryPoint
 * @brief Represents a point on a trajectory.
 *
 * This class encapsulates information about a point on a trajectory, such as its position,
 * momentum, process, and subprocess.
 */
class EDEPTrajectoryPoint {
 public:
  /**
   * @brief Constructor for EDEPTrajectoryPoint.
   * @param trajectory_hit The TG4TrajectoryPoint object containing hit information.
   */
  explicit EDEPTrajectoryPoint(const TG4TrajectoryPoint& p)
    : position_(p.GetPosition()),
      momentum_(p.GetMomentum().X(), p.GetMomentum().Y(), p.GetMomentum().Z()),
      process_(p.GetProcess()),
      sub_process_(p.GetSubprocess()) {}

  /**
   * @brief Destructor for EDEPTrajectoryPoint.
   */
  ~EDEPTrajectoryPoint() {};

  bool operator== (const EDEPTrajectoryPoint& p) const {
    return position_ == p.position_ && momentum_ == p.momentum_ && process_ == p.process_
          && sub_process_ == p.sub_process_;
  };

  /**
   * @brief Get the position of the trajectory point.
   * @return The position as a XYZTVector.
   */
  const sand::vec_4d& GetPosition() const { return position_; };

  /**
   * @brief Get the momentum of the trajectory point.
   * @return The momentum as a XYZVector.
   */
  const sand::mom_3d& GetMomentum() const { return momentum_; };

  /**
   * @brief Get the process associated with the trajectory point.
   * @return The process as an integer.
   */
  const int& GetProcess() const { return process_; };

  /**
   * @brief Get the subprocess associated with the trajectory point.
   * @return The subprocess as an integer.
   */
  const int& GetSubprocess() const { return sub_process_; };

 private:
  sand::vec_4d position_;   ///< Position of the trajectory point.
  sand::mom_3d momentum_;   ///< Momentum of the trajectory point.
  int process_;             ///< Process associated with the trajectory point.
  int sub_process_;         ///< Subprocess associated with the trajectory point.
};

using EDEPTrajectoryPoints = std::map<sand::subdetector_t, std::vector<EDEPTrajectoryPoint>>;
