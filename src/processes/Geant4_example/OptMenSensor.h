/*
 * File:   OptMenSensor.h
 * Author: pozzato
 *
 * Created on April 1, 2014, 12:14 PM
 */

#ifndef OptMenSENSOR_H
#define OptMenSENSOR_H

#include "OptMenSensorHit.h"
#include "G4ProcessManager.hh"
#include "G4VSensitiveDetector.hh"

class G4Step;
class G4HCofThisEvent;

class OptMenSensor : public G4VSensitiveDetector {
 public:
  OptMenSensor(const G4String &name, const G4String &hitsCollectionName);
  virtual ~OptMenSensor();
  virtual void Initialize(G4HCofThisEvent *hitCollection);

  virtual G4bool ProcessHits(G4Step *aStep, G4TouchableHistory *history);

  virtual void EndOfEvent(G4HCofThisEvent *hitCollection);
  // A version of processHits that keeps aStep constant
  // G4bool processHitsConstStep(const G4Step *aStep, G4TouchableHistory
  // *history);
  G4int nHits;
  G4int collectionID;

 private:
  OptMenSensorHitCollection *_photonDetHitCollection;
};

#endif /* OptMenSENSOR_H */
