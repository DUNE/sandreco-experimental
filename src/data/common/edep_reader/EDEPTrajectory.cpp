#include "EDEPTrajectory.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <ufw/utils.hpp>

namespace {
  bool near_4d(const EDEPHit& h, const sand::vec_4d& point, double distance, double time) {
    sand::pos_3d mid = sand::pos_3d(h.GetStart().Vect() + h.GetStop().Vect()) * 0.5;
    double mid_time  = (h.GetStart().T() + h.GetStop().T()) * 0.5;
    return std::sqrt((mid - point.Vect()).Mag2()) < distance && std::fabs(mid_time - point.T()) < time;
  }
} // namespace

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
      std::cout << sand::component_to_string.at(el.first) << " " << el.second.size() << "; " << std::endl;
      full_out += sand::component_to_string.at(el.first) + " " + std::to_string(el.second.size()) + "; \n";
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

/**
 * @brief Checks if the trajectory has hits before a specified time.
 * @param time The time threshold.
 * @return True if the trajectory has hits before the specified time, otherwise false.
 */
bool EDEPTrajectory::HasHitBeforeTime(double t) const {
  return HasHitWhere([t](const EDEPHit& h) { return h.GetStart().T() < t; });
}

/**
 * @brief Checks if the trajectory has hits with energy smaller than the specified amount.
 * @param time The energy threshold.
 * @return True if the trajectory has hits with energy smaller than the specified amount, otherwise false.
 */
bool EDEPTrajectory::HasHitWithEnergySmallerThan(double e) const {
  return HasHitWhere([e](const EDEPHit& h) { return h.GetEnergyDeposit() < e; });
}

/**
 * @brief Checks if the trajectory has hits with energy larger than the specified amount.
 * @param time The energy threshold.
 * @return True if the trajectory has hits with energy larger than the specified amount, otherwise false.
 */
bool EDEPTrajectory::HasHitWithEnergyLargerThan(double e) const {
  return HasHitWhere([e](const EDEPHit& h) { return h.GetEnergyDeposit() > e; });
}

/**
 * @brief Checks if the trajectory has hits after a specified time.
 * @param time The time threshold.
 * @return True if the trajectory has hits after the specified time, otherwise false.
 */
bool EDEPTrajectory::HasHitAfterTime(double t) const {
  return HasHitWhere([t](const EDEPHit& h) { return h.GetStart().T() > t; });
}

/**
 * @brief Checks if the trajectory has hits within a specified time range.
 * @param start_time The start of the time range.
 * @param stop_time The end of the time range.
 * @return True if the trajectory has hits within the specified time range, otherwise false.
 */
bool EDEPTrajectory::HasHitInTime(double t0, double t1) const {
  return HasHitWhere([t0, t1](const EDEPHit& h) { 
    return (h.GetStart().T() > t0 && h.GetStart().T() < t1);
  });
}

/**
 * @brief Checks if the trajectory has hits within a specified energy range.
 * @param min The min of the energy range.
 * @param max The max of the energy range.
 * @return True if the trajectory has hits within the specified energy range, otherwise false.
 */
bool EDEPTrajectory::HasHitInEnergy(double min, double max) const {
  return HasHitWhere([min, max](const EDEPHit& h) {
    return (h.GetEnergyDeposit() > min && h.GetEnergyDeposit() < max);
  });
}

/**
 * @brief Checks if the trajectory has hits within a specified energy and time ranges.
 * @param start_time The start of the time range.
 * @param stop_time The stop of the time range.
 * @param min_energy The min of the energy range.
 * @param max_energy The max of the energy range.
 * @return True if the trajectory has hits within the specified energy range, otherwise false.
 */
bool EDEPTrajectory::HasHitInTimeAndEnergy(double t0, double t1, double emin, double emax) const {
  return HasHitWhere([=](const EDEPHit& h) {
    return (h.GetEnergyDeposit() > emin && h.GetEnergyDeposit() < emax && 
            h.GetStart().T() > t0 && h.GetStart().T() < t1);
  });
}

/**
 * @brief Checks if any hit is within a given distance from a point.
 *
 * This function determines if the midpoint of any hit is closer than the
 * specified distance to the given point.
 *
 * @param point The reference point as a sand::pos_3d.
 * @param distance The maximum distance to consider.
 * @return true if a hit is within the specified distance from the point, false otherwise.
 */
bool EDEPTrajectory::HasHitNearPoint(sand::pos_3d point, double distance) const {
  return HasHitWhere([&](const EDEPHit& h) {
    sand::pos_3d mid = sand::pos_3d(h.GetStart().Vect() + h.GetStop().Vect()) * 0.5;
    return std::sqrt((mid - point).Mag2()) < distance;
  });
}

/**
 * @brief Checks if any hit is within a given 4D distance (space and time) from a point.
 *
 * This function checks if the midpoint of any hit is within the specified spatial
 * distance and time difference from a given 4D point.
 *
 * @param point The reference 4D point as a sand::vec_4d (includes both space and time).
 * @param distance The maximum spatial distance to consider.
 * @param time The maximum time difference to consider.
 * @return true if a hit is within the specified space and time distance from the point, false otherwise.
 */
bool EDEPTrajectory::HasHitNear4DPoint(sand::vec_4d point, double distance, double time) const {
  return HasHitWhere([&](const EDEPHit& h) { return near_4d(h, point, distance, time); });
}

/**
 * @brief Returns an iterator to the hit near a given 4D point (space and time).
 *
 * This function searches for a hit whose midpoint is within the specified spatial
 * distance and time difference from the given 4D point. It returns an iterator
 * to the first hit that matches the criteria.
 *
 * @param point The reference 4D point as a sand::vec_4d (includes space and time components).
 * @param distance The maximum allowed spatial distance.
 * @param time The maximum allowed time difference.
 * @return An iterator to the first hit that is near the specified 4D point, or the end iterator if no hit matches.
 */
const EDEPHit* EDEPTrajectory::GetHitNear4DPoint(sand::vec_4d point, double distance, double time) const {
  return GetHitWhere([&](const EDEPHit& h) { return near_4d(h, point, distance, time); });
}

/**
 * @brief Returns the hit in the trajectory with the specified ID.
 *
 * This function searches through all hits in the trajectory and returns the one that has a matching ID.
 *
 * @param id The ID to search for.
 * @return the hit with the specified ID.
 */
const EDEPHit& EDEPTrajectory::GetHitWithId(int id) const {
  const EDEPHit* hit = FindHit(id);
  if (hit == nullptr) {
    UFW_ERROR("Hit with id {} not found", id);
  }
  return *hit;
}

/**
 * @brief Checks if the trajectory has a hit with the specified ID in the specified detector sand::subdetector_t.
 * @param id The ID of the hit.
 * @param component_name The name of the detector sand::subdetector_t.
 * @return True if the trajectory has a hit with the specified ID in the specified sand::subdetector_t, otherwise false.
 */
bool EDEPTrajectory::HasHitWithIdInDetector(int id, sand::subdetector_t c) const {
  auto it = hit_map_.find(c);
  if (it == hit_map_.end()) {
    return false;
  }
  return std::any_of(it->second.begin(), it->second.end(), [id](const EDEPHit& h) { return h.GetId() == id; });
}

/**
 * @brief Calculates the total deposited energy in the specified detector sand::subdetector_t.
 * @param component_name The name of the detector sand::subdetector_t.
 * @return The total deposited energy in the specified sand::subdetector_t.
 */
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

/**
 * @brief Checks if the trajectory is saturated (i.e., has reached the maximum number of points).
 * @return True if the trajectory is saturated, otherwise false.
 */
bool EDEPTrajectory::IsTrajectorySaturated() const {
  std::size_t n = 0;
  for (const auto& comp : trajectory_points_) {
    n += comp.second.size();
  }
  return n >= kMaxPoints;
}

/**
 * @brief Matches a given volume name against a list of names.
 * @param volume The volume name to match.
 * @param names An initializer list of names to match against.
 * @return True if the volume name matches any of the names in the list, otherwise false.
 */
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
  std::stable_sort(points.begin(), points.end(), [](const EDEPTrajectoryPoint& a, const EDEPTrajectoryPoint& b) { return a.GetPosition().T() < b.GetPosition().T();});

  return points;
}
