#include "EDEPTrajectory.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <ufw/utils.hpp>

namespace sand::edep_refactor {

  bool EDEPTrajectory::operator== (const EDEPTrajectory& trj) const {
    return id_ == trj.id_ && parent_id_ == trj.parent_id_ && parent_trajectory_ == trj.parent_trajectory_
        && pdg_code_ == trj.pdg_code_ && p0_ == trj.p0_ && depth_ == trj.depth_
        && interaction_number_ == trj.interaction_number_ && reaction_ == trj.reaction_ && entering_ == trj.entering_
        && exiting_ == trj.exiting_;
  }

  std::string EDEPTrajectory::Print(std::string& full_out, int depth, int current_depth) const {
    if (current_depth > depth) {
      return full_out;
    }
    for (int i = 0; i < current_depth; i++) {
      const char* pad = (i != current_depth - 1) ? "    " : "|-- ";
      std::cout << pad;
      full_out += pad;
    }
    std::cout << GetDepth() << " " << GetId() << " " << GetInteractionNumber() << " " << this << " " << GetPDGCode()
              << std::endl;
    if (GetDepth() == 0) {
      std::cout << GetReaction() << std::endl;
    }
    full_out += std::to_string(GetDepth()) + " " + std::to_string(GetId()) + " " + std::to_string(GetPDGCode()) + "\n";

    for (const auto& el : hit_map_) {
      for (int i = 0; i < current_depth; i++) {
        std::cout << "    ";
        full_out += "    ";
      }
      std::cout << sand::component_to_string[el.first] << " " << el.second.size() << "; " << std::endl;
      full_out += sand::component_to_string[el.first] + " " + std::to_string(el.second.size()) + "; \n";
    }
    for (const auto* child : children_) {
      child->Print(full_out, depth, current_depth + 1);
    }
    return full_out;
  }

  const EDEPHit* EDEPTrajectory::FindHit(int id) const {
    for (const auto& hits : hit_map_) {
      for (const auto& hit : hits.second) {
        if (hit.GetId() == id) {
          return &hit;
        }
      }
    }
    return nullptr;
  }

  const EDEPHit& EDEPTrajectory::GetHitWithId(int id) const {
    const EDEPHit* hit = FindHit(id);
    if (hit == nullptr) {
      UFW_ERROR("Hit with id {} not found", id);
    }
    return *hit;
  }

  bool EDEPTrajectory::HasHitWithIdInDetector(int id, sand::subdetector_t c) const {
    auto it = hit_map_.find(c);
    if (it == hit_map_.end()) {
      return false;
    }
    return std::any_of(it->second.begin(), it->second.end(), [id](const EDEPHit& h) { return h.GetId() == id; });
  }

  double EDEPTrajectory::GetDepositedEnergy(sand::subdetector_t c) const {
    auto it = hit_map_.find(c);
    if (it == hit_map_.end()) {
      return 0.;
    }
    double e = 0.;
    for (const auto& hit : it->second) {
      e += hit.GetSecondaryDeposit();
    }
    return e;
  }

  bool EDEPTrajectory::HasHitBeforeTime(double t) const {
    return HasHitWhere([t](const EDEPHit& h) { return h.GetStart().T() < t; });
  }

  bool EDEPTrajectory::HasHitAfterTime(double t) const {
    return HasHitWhere([t](const EDEPHit& h) { return h.GetStart().T() > t; });
  }

  bool EDEPTrajectory::HasHitWithEnergySmallerThan(double e) const {
    return HasHitWhere([e](const EDEPHit& h) { return h.GetEnergyDeposit() < e; });
  }

  bool EDEPTrajectory::HasHitWithEnergyLargerThan(double e) const {
    return HasHitWhere([e](const EDEPHit& h) { return h.GetEnergyDeposit() > e; });
  }

  bool EDEPTrajectory::HasHitInEnergy(double min, double max) const {
    return HasHitWhere([min, max](const EDEPHit& h) { return h.GetEnergyDeposit() > min && h.GetEnergyDeposit() < max; });
  }

  bool EDEPTrajectory::HasHitInTime(double t0, double t1) const {
    return HasHitWhere([t0, t1](const EDEPHit& h) { return h.GetStart().T() > t0 && h.GetStart().T() < t1; });
  }

  bool EDEPTrajectory::HasHitInTimeAndEnergy(double t0, double t1, double emin, double emax) const {
    return HasHitWhere([=](const EDEPHit& h) {
      return h.GetEnergyDeposit() > emin && h.GetEnergyDeposit() < emax && h.GetStart().T() > t0
          && h.GetStart().T() < t1;
    });
  }

  bool EDEPTrajectory::HasHitNearPoint(sand::pos_3d point, double distance) const {
    return HasHitWhere([&](const EDEPHit& h) {
      sand::pos_3d mid = sand::pos_3d(h.GetStart().Vect() + h.GetStop().Vect()) * 0.5;
      return std::sqrt((mid - point).Mag2()) < distance;
    });
  }

  namespace {
    bool near_4d(const EDEPHit& h, const sand::vec_4d& point, double distance, double time) {
      sand::pos_3d mid = sand::pos_3d(h.GetStart().Vect() + h.GetStop().Vect()) * 0.5;
      double mid_time  = (h.GetStart().T() + h.GetStop().T()) * 0.5;
      return std::sqrt((mid - point.Vect()).Mag2()) < distance && std::fabs(mid_time - point.T()) < time;
    }
  } // namespace

  bool EDEPTrajectory::HasHitNear4DPoint(sand::vec_4d point, double distance, double time) const {
    return HasHitWhere([&](const EDEPHit& h) { return near_4d(h, point, distance, time); });
  }

  const EDEPHit* EDEPTrajectory::GetHitNear4DPoint(sand::vec_4d point, double distance, double time) const {
    return GetHitWhere([&](const EDEPHit& h) { return near_4d(h, point, distance, time); });
  }

  bool EDEPTrajectory::IsTrajectorySaturated() const {
    std::size_t n = 0;
    for (const auto& comp : trajectory_points_) {
      n += comp.second.size();
    }
    return n >= kMaxPoints;
  }

  bool EDEPTrajectory::Match(std::string_view volume, std::initializer_list<std::string> names) {
    for (const auto& n : names) {
      if (volume.find(n) != std::string_view::npos) {
        return true;
      }
    }
    return false;
  }

  std::vector<EDEPTrajectoryPoint> EDEPTrajectory::GetTrajectoryPointsVect() const {
    std::size_t n = 0;
    for (const auto& comp : trajectory_points_) {
      n += comp.second.size();
    }
    std::vector<EDEPTrajectoryPoint> points;
    points.reserve(n);
    for (const auto& comp : trajectory_points_) {
      points.insert(points.end(), comp.second.begin(), comp.second.end());
    }
    std::stable_sort(points.begin(), points.end(), [](const EDEPTrajectoryPoint& a, const EDEPTrajectoryPoint& b) {
      return a.GetPosition().T() < b.GetPosition().T();
    });
    return points;
  }

} // namespace sand::edep_refactor
