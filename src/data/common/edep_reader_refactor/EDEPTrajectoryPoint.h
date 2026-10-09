#pragma once

#include <EDepSim/TG4Event.h>
#include <common/sand.h>

#include <map>
#include <vector>

namespace sand::edep_refactor {

  /**
   * @class EDEPTrajectoryPoint
   * @brief Represents a point on a trajectory (position, momentum, process, subprocess).
   */
  class EDEPTrajectoryPoint {
   public:
    explicit EDEPTrajectoryPoint(const TG4TrajectoryPoint& p)
      : position_(p.GetPosition()),
        momentum_(p.GetMomentum().X(), p.GetMomentum().Y(), p.GetMomentum().Z()),
        process_(p.GetProcess()),
        sub_process_(p.GetSubprocess()) {}

    bool operator== (const EDEPTrajectoryPoint& p) const {
      return position_ == p.position_ && momentum_ == p.momentum_ && process_ == p.process_
          && sub_process_ == p.sub_process_;
    }

    const sand::vec_4d& GetPosition() const { return position_; }
    const sand::mom_3d& GetMomentum() const { return momentum_; }
    const int& GetProcess() const { return process_; }
    const int& GetSubprocess() const { return sub_process_; }

   private:
    sand::vec_4d position_;
    sand::mom_3d momentum_;
    int process_;
    int sub_process_;
  };

  using EDEPTrajectoryPoints = std::map<sand::subdetector_t, std::vector<EDEPTrajectoryPoint>>;

} // namespace sand::edep_refactor
