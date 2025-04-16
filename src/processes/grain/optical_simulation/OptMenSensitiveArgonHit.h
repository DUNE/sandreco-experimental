/*
 * File:   OptMenSensitiveArgonHit.h
 * Author: pozzato
 *
 * Created on April 11, 2014, 12:42 PM
 */

#ifndef OptMenSensitiveArgonHit_H
#define OptMenSensitiveArgonHit_H

#include "G4Allocator.hh"
#include "G4THitsCollection.hh"
#include "G4VHit.hh"

#include "G4RotationMatrix.hh"
#include "G4ThreeVector.hh"
#include "G4Transform3D.hh"

#include "G4LogicalVolume.hh"
#include "G4VPhysicalVolume.hh"

#include "tls.hh"

class G4VTouchable;

class OptMenSensitiveArgonHit : public G4VHit {
 public:
  OptMenSensitiveArgonHit();
  OptMenSensitiveArgonHit(G4int trackID, G4int pdgCode, G4double depositedEnergy, G4ThreeVector hitPosition);
  virtual ~OptMenSensitiveArgonHit();

  const OptMenSensitiveArgonHit& operator=(const OptMenSensitiveArgonHit& right);

  G4int operator==(const OptMenSensitiveArgonHit& right) const;

  inline void* operator new(size_t);
  inline void operator delete(void* aHit);

  inline void energy(G4double e) { _depositedEnergy = e; };
  inline G4double energy() const { return _depositedEnergy; };

  inline void pdgCode(G4int c) { _pdgCode = c; };
  inline G4int pdgCode() const { return _pdgCode; };

  inline void trackID(G4int c) { _trackID = c; };
  inline G4int trackID() const { return _trackID; };

  inline void hitPosition(G4ThreeVector h) { _hitPosition = h; };
  inline G4ThreeVector hitPosition() const { return _hitPosition; };

 private:
    G4double _depositedEnergy;
    G4int _pdgCode;
    G4int _trackID;
    G4ThreeVector _hitPosition;
};

//--------------------------------------------------
// Type Definitions
//--------------------------------------------------

typedef G4THitsCollection<OptMenSensitiveArgonHit> OptMenSensitiveArgonHitCollection;

extern G4ThreadLocal G4Allocator<OptMenSensitiveArgonHit>* OptMenSensitiveArgonHitAllocator;

//--------------------------------------------------
// Operator Overloads
//--------------------------------------------------

inline void* OptMenSensitiveArgonHit::operator new(size_t) {
  if (!OptMenSensitiveArgonHitAllocator) OptMenSensitiveArgonHitAllocator = new G4Allocator<OptMenSensitiveArgonHit>;
  return (void*)OptMenSensitiveArgonHitAllocator->MallocSingle();
}

inline void OptMenSensitiveArgonHit::operator delete(void* aHit) {
  OptMenSensitiveArgonHitAllocator->FreeSingle((OptMenSensitiveArgonHit*)aHit);
}

#endif /* OptMenSensitiveArgonHit_H */
