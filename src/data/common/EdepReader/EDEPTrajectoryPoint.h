#include <iostream>

#include "TG4Event.h"

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
    EDEPTrajectoryPoint(const TG4TrajectoryPoint& trajectory_hit) : position_(trajectory_hit.GetPosition()), momentum_(trajectory_hit.GetMomentum()),
                                                            process_(trajectory_hit.GetProcess()), sub_process_(trajectory_hit.GetSubprocess()) {};
    
    /**
     * @brief Destructor for EDEPTrajectoryPoint.
     */
    ~EDEPTrajectoryPoint() {};

    bool operator==(const EDEPTrajectoryPoint& p) {
      return (
          this->position_ == p.GetPosition()   &&
        this->momentum_ == p.GetMomentum()     &&
        this->process_ == p.GetProcess()       &&
        this->sub_process_ == p.GetSubprocess()
      );
    };

    /**
     * @brief Get the position of the trajectory point.
     * @return The position as a TLorentzVector.
     */
    const TLorentzVector& GetPosition()    const {return position_;};
    
    /**
     * @brief Get the momentum of the trajectory point.
     * @return The momentum as a TVector3.
     */
    const TVector3&       GetMomentum()    const {return momentum_;};
    
    /**
     * @brief Get the process associated with the trajectory point.
     * @return The process as an integer.
     */
    const int&    GetProcess()     const {return process_;};
    
    /**
     * @brief Get the subprocess associated with the trajectory point.
     * @return The subprocess as an integer.
     */
    const int&    GetSubprocess()  const {return sub_process_;};
  
  private:
    TLorentzVector position_;   ///< Position of the trajectory point.
    TVector3 momentum_;        ///< Momentum of the trajectory point.
    int process_;              ///< Process associated with the trajectory point.
    int sub_process_;          ///< Subprocess associated with the trajectory point.
};


using EDEPTrajectoryPoints = std::map<component, std::vector<EDEPTrajectoryPoint>>;
