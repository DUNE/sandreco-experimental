/*
 * File:   SensitiveArgonHit.cpp
 * Author: pozzato
 *
 * Created on April 11, 2014, 12:42 PM
 */

#include "SensitiveArgonHit.h"

G4ThreadLocal G4Allocator<SensitiveArgonHit>* SensitiveArgonHitAllocator = 0;

SensitiveArgonHit::SensitiveArgonHit() {
  _depositedEnergy = 0;
  _trackID = -1;
  _pdgCode = -1;
  _hitPosition = G4ThreeVector(0., 0., 0.);
}

SensitiveArgonHit::SensitiveArgonHit(G4int pTrackID, G4int pPdgCode, G4double pDepositedEnergy, G4ThreeVector pHitPosition) {
  _depositedEnergy = pDepositedEnergy;
  _pdgCode = pPdgCode;
  _trackID = pTrackID;
  _hitPosition = pHitPosition;
}

SensitiveArgonHit::~SensitiveArgonHit() {}

const SensitiveArgonHit& SensitiveArgonHit::operator=(const SensitiveArgonHit& right) {
  _depositedEnergy = right._depositedEnergy;
  _pdgCode = right._pdgCode;
  _trackID = right._trackID;
  _hitPosition = right._hitPosition;
  return *this;
}

G4int SensitiveArgonHit::operator==(const SensitiveArgonHit& right) const {
  return (_depositedEnergy == right._depositedEnergy && _pdgCode == right._pdgCode &&
          _trackID == right._trackID && _hitPosition == right._hitPosition);
}
