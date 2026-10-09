#pragma once

#include <EDepSim/TG4Event.h>
#include <common/sand.h>

#include <map>
#include <vector>

namespace sand::edep_refactor {

  /**
   * @class EDEPHit
   * @brief Represents a hit in a detector.
   *
   * Same interface as the legacy EDEPHit, but constructors take their arguments by const reference.
   */
  class EDEPHit {
   public:
    EDEPHit() = default;

    explicit EDEPHit(const TG4HitSegment& hit, int i = -1)
      : start_(hit.GetStart()),
        stop_(hit.GetStop()),
        energy_deposit_(hit.GetEnergyDeposit()),
        secondary_deposit_(hit.GetSecondaryDeposit()),
        track_length_(hit.GetTrackLength()),
        contrib_(hit.Contrib[0]),
        primary_id_(hit.GetPrimaryId()),
        h_index(i) {}

    EDEPHit(const sand::vec_4d& start, const sand::vec_4d& stop, double energy_deposit, double secondary_deposit,
            double track_length, int contrib, int primary_id, int i)
      : start_(start),
        stop_(stop),
        energy_deposit_(energy_deposit),
        secondary_deposit_(secondary_deposit),
        track_length_(track_length),
        contrib_(contrib),
        primary_id_(primary_id),
        h_index(i) {}

    const sand::vec_4d& GetStart() const { return start_; }
    const sand::vec_4d& GetStop() const { return stop_; }
    const double& GetEnergyDeposit() const { return energy_deposit_; }
    const double& GetSecondaryDeposit() const { return secondary_deposit_; }
    const double& GetTrackLength() const { return track_length_; }
    const int& GetContrib() const { return contrib_; }
    const int& GetPrimaryId() const { return primary_id_; }
    const int& GetId() const { return h_index; }

   private:
    sand::vec_4d start_{};
    sand::vec_4d stop_{};
    double energy_deposit_{};
    double secondary_deposit_{};
    double track_length_{};
    int contrib_{-1};
    int primary_id_{-1};
    int h_index{-1};
  };

  /// Map of subdetector to the hits it contains.
  using EDEPHitsMap = std::map<sand::subdetector_t, std::vector<EDEPHit>>;

} // namespace sand::edep_refactor
