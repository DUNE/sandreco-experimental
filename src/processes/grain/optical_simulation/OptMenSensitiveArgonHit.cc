/*
 * File:   OptMenSensitiveArgonHit.cpp
 * Author: pozzato
 *
 * Created on April 11, 2014, 12:42 PM
 */

#include "OptMenSensitiveArgonHit.h"

G4ThreadLocal G4Allocator<OptMenSensitiveArgonHit>* OptMenSensitiveArgonHitAllocator = 0;

OptMenSensitiveArgonHit::OptMenSensitiveArgonHit() {
  _depositedEnergy = 0;
  _trackID = -1;
  _pdgCode = -1;
  _hitPosition = G4ThreeVector(0., 0., 0.);
}

OptMenSensitiveArgonHit::OptMenSensitiveArgonHit(G4int pTrackID, G4int pPdgCode, G4double pDepositedEnergy, G4ThreeVector pHitPosition) {
  _depositedEnergy = pDepositedEnergy;
  _pdgCode = pPdgCode;
  _trackID = pTrackID;
  _hitPosition = pHitPosition;
}

OptMenSensitiveArgonHit::~OptMenSensitiveArgonHit() {}

const OptMenSensitiveArgonHit& OptMenSensitiveArgonHit::operator=(const OptMenSensitiveArgonHit& right) {
  _depositedEnergy = right._depositedEnergy;
  _pdgCode = right._pdgCode;
  _trackID = right._trackID;
  _hitPosition = right._hitPosition;
  return *this;
}

G4int OptMenSensitiveArgonHit::operator==(const OptMenSensitiveArgonHit& right) const {
  return (_depositedEnergy == right._depositedEnergy && _pdgCode == right._pdgCode &&
          _trackID == right._trackID && _hitPosition == right._hitPosition);
}
