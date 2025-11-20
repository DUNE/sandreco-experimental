/*
 * File:   SensitiveArgonHit.h
 * Author: pozzato
 *
 * Created on April 11, 2014, 12:42 PM
 */

#ifndef SensitiveArgonHit_H
#define SensitiveArgonHit_H

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

namespace sand::grain {
  class SensitiveArgonHit : public G4VHit {
   public:
    SensitiveArgonHit();
    SensitiveArgonHit(G4int trackID, G4int pdgCode, G4double depositedEnergy, G4ThreeVector hitPosition);
    virtual ~SensitiveArgonHit();

    const SensitiveArgonHit& operator= (const SensitiveArgonHit& right);

    G4int operator== (const SensitiveArgonHit& right) const;

    inline void* operator new (size_t);
    inline void operator delete (void* aHit);

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

  typedef G4THitsCollection<SensitiveArgonHit> SensitiveArgonHitCollection;

  extern G4ThreadLocal G4Allocator<SensitiveArgonHit>* SensitiveArgonHitAllocator;

  //--------------------------------------------------
  // Operator Overloads
  //--------------------------------------------------

  inline void* SensitiveArgonHit::operator new (size_t) {
    if (!SensitiveArgonHitAllocator)
      SensitiveArgonHitAllocator = new G4Allocator<SensitiveArgonHit>;
    return (void*)SensitiveArgonHitAllocator->MallocSingle();
  }

  inline void SensitiveArgonHit::operator delete (void* aHit) {
    SensitiveArgonHitAllocator->FreeSingle((SensitiveArgonHit*)aHit);
  }
} // namespace sand::grain
#endif /* SensitiveArgonHit_H */