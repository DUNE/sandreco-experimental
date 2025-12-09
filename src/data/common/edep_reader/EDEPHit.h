#pragma once
#include <edep_reader/EDEPUtils.h>

#include <TG4Event.h>

/**
 * @class EDEPHit
 * @brief Represents a hit in a detector.
 *
 * This class encapsulates information about a hit in a detector, such as its start and stop positions,
 * energy deposition, secondary deposition, track length, contribution, primary ID, and index.
 */
class EDEPHit {
 public:
  /**
   * @brief Constructor for EDEPHit.
   * @param hit The TG4HitSegment object containing hit information.
   */
  EDEPHit() = default;
  
  EDEPHit(TG4HitSegment hit)
    : start_(hit.GetStart()),
      stop_(hit.GetStop()),
      energy_deposit_(hit.GetEnergyDeposit()),
      secondary_deposit_(hit.GetSecondaryDeposit()),
      track_length_(hit.GetTrackLength()),
      contrib_(hit.Contrib[0]),
      primary_id_(hit.GetPrimaryId()),
      h_index(-1) {};
  /**
   * @brief Constructor for EDEPHit.
   * @param hit The TG4HitSegment object containing hit information.
   * @param i The index of the hit.
   */
  EDEPHit(TG4HitSegment hit, int i)
    : start_(hit.GetStart()),
      stop_(hit.GetStop()),
      energy_deposit_(hit.GetEnergyDeposit()),
      secondary_deposit_(hit.GetSecondaryDeposit()),
      track_length_(hit.GetTrackLength()),
      contrib_(hit.Contrib[0]),
      primary_id_(hit.GetPrimaryId()),
      h_index(i) {};
  /**
   * @brief Constructor for EDEPHit.
   * @param start The start position of the hit.
   * @param stop The stop position of the hit.
   * @param energy_deposit The energy deposited by the hit.
   * @param secondary_deposit The secondary deposition of the hit.
   * @param track_length The track length of the hit.
   * @param contrib The main contributor of the hit.
   * @param primary_id The ID of the primary particle generating the hit.
   * @param i The index of the hit.
   */
  EDEPHit(TLorentzVector start,
          TLorentzVector stop,
          double energy_deposit,
          double secondary_deposit,
          double track_length,
          int contrib,
          int primary_id,
          int i)
    : start_(start),
      stop_(stop),
      energy_deposit_(energy_deposit),
      secondary_deposit_(secondary_deposit),
      track_length_(track_length),
      contrib_(contrib),
      primary_id_(primary_id),
      h_index(i) {};
  /**
   * @brief Destructor for EDEPHit.
   */
  ~EDEPHit() {};

  /**
   * @brief Get the start position of the hit.
   * @return The start position as a TLorentzVector.
   */
  const TLorentzVector& GetStart() const { return start_; };

  /**
   * @brief Get the stop position of the hit.
   * @return The stop position as a TLorentzVector.
   */
  const TLorentzVector& GetStop() const { return stop_; };

  /**
   * @brief Get the energy deposited by the hit.
   * @return The energy deposited as a double.
   */
  const double& GetEnergyDeposit() const { return energy_deposit_; };

  /**
   * @brief Get the secondary deposition of the hit.
   * @return The secondary deposition as a double.
   */
  const double& GetSecondaryDeposit() const { return secondary_deposit_; };

  /**
   * @brief Get the track length of the hit.
   * @return The track length as a double.
   */
  const double& GetTrackLength() const { return track_length_; };

  /**
   * @brief Get the main contributor to the hit.
   * @return The contribution as an integer.
   */
  const int& GetContrib() const { return contrib_; };

  /**
   * @brief Get the ID of the primary particle generating the hit.
   * @return The primary ID as an integer.
   */
  const int& GetPrimaryId() const { return primary_id_; };

  /**
   * @brief Get the ID of the hit.
   * @return The ID as an integer.
   */
  const int& GetId() const { return h_index; };

 private:
  TLorentzVector start_;     ///< Start position of the hit.
  TLorentzVector stop_;      ///< Stop position of the hit.
  double energy_deposit_;    ///< Energy deposited by the hit.
  double secondary_deposit_; ///< Secondary deposition of the hit.
  double track_length_;      ///< Track length of the hit.
  int contrib_;              ///< Main contributor of the hit.
  int primary_id_;           ///< ID of primary particle generating the hit.
  int h_index;               ///< ID of the hit.
};

/**
 * @brief Alias for a map of component to vector of EDEPHit.
 */
using EDEPHitsMap = std::map<component, std::vector<EDEPHit>>;
