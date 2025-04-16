/*
 * File:   OptMenSensorHit.h
 * Author: pozzato
 *
 * Created on April 11, 2014, 12:42 PM
 */

#ifndef OptMenSENSORHIT_H
#define OptMenSENSORHIT_H

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

class OptMenSensorHit : public G4VHit {
 public:
  OptMenSensorHit();
  OptMenSensorHit(G4ThreeVector pArrive, G4ThreeVector pOrigin, G4ThreeVector pDirection, G4double pTime, G4double pEnergy, G4double pScatter, G4String camName, G4String productionVolume);
  OptMenSensorHit(const OptMenSensorHit& orig);
  virtual ~OptMenSensorHit();

  const OptMenSensorHit& operator=(const OptMenSensorHit& right);

  G4int operator==(const OptMenSensorHit& right) const;

  inline void* operator new(size_t);
  inline void operator delete(void* aHit);

  inline void arrivalPos(G4ThreeVector xyz) { _posArrive = xyz; }
  inline G4ThreeVector arrivalPos() const { return _posArrive; }

  inline void originPos(G4ThreeVector xyz) { _posOrigin = xyz; }
  inline G4ThreeVector originPos() const { return _posOrigin; }

  inline void direction(G4ThreeVector pxpypz) { _direction = pxpypz; }
  inline G4ThreeVector direction() const { return _direction; }

  inline void directionOrigin(G4ThreeVector pxpypzOrigin) { _directionOrigin = pxpypzOrigin; }
  inline G4ThreeVector directionOrigin() const { return _directionOrigin; }

  inline void arrivalTime(G4double t) { _arrivalTime = t; }
  inline G4double arrivalTime() const { return _arrivalTime; }

  inline void energy(G4double e) { _energy = e; };
  inline G4double energy() const { return _energy; };

  inline void scatter(G4double s) { _scatter = s; };
  inline G4double scatter() const { return _scatter; };

  inline void camName(G4String c) { _camName = c; };
  inline G4String camName() const { return _camName; };

  inline void productionVolume(G4String c) { _productionVolume = c; };
  inline G4String productionVolume() const { return _productionVolume; };

 private:
  // the arrival time of the photon
  G4double _arrivalTime;
  // where the photon hit the detector (detector's coordinate)
  G4ThreeVector _posArrive;
  // where the photon has been emitted (global's coordinate)
  G4ThreeVector _posOrigin;
  // the direction of the photon 
  G4ThreeVector _direction;
  // the direction of the photon at the origin
  G4ThreeVector _directionOrigin;
  // energy of photons
  G4double _energy;
  // scatter angle of photons
  G4double _scatter;
  // name of the camera
  G4String _camName;
  // name of the volume in which the photon has been emitted
  G4String _productionVolume;
};

//--------------------------------------------------
// Type Definitions
//--------------------------------------------------

typedef G4THitsCollection<OptMenSensorHit> OptMenSensorHitCollection;

extern G4ThreadLocal G4Allocator<OptMenSensorHit>* OptMenSensorHitAllocator;

//--------------------------------------------------
// Operator Overloads
//--------------------------------------------------

inline void* OptMenSensorHit::operator new(size_t) {
  if (!OptMenSensorHitAllocator) OptMenSensorHitAllocator = new G4Allocator<OptMenSensorHit>;
  return (void*)OptMenSensorHitAllocator->MallocSingle();
}

inline void OptMenSensorHit::operator delete(void* aHit) {
  OptMenSensorHitAllocator->FreeSingle((OptMenSensorHit*)aHit);
}

#endif /* OptMenSENSORHIT_H */
