/*
 * File:   OptMenSensorHit.cpp
 * Author: pozzato
 *
 * Created on April 11, 2014, 12:42 PM
 */

#include "OptMenSensorHit.h"

G4ThreadLocal G4Allocator<OptMenSensorHit>* OptMenSensorHitAllocator = 0;

OptMenSensorHit::OptMenSensorHit() {
  _arrivalTime = 0.;
  _posArrive = G4ThreeVector(0., 0., 0.);
  _direction = G4ThreeVector(0., 0., 0.);
  _energy = 0;
  _scatter = 0;
  _camName = "NULL_CAM";
  _productionVolume = "NULL_VOLUME";
}

OptMenSensorHit::OptMenSensorHit(G4ThreeVector pArrive, G4ThreeVector pOrigin, G4ThreeVector pDirection, G4double pTime,
                       G4double pEnergy, G4double pScatter, G4String pCamName, G4String pProductionVolume) {
  _arrivalTime = pTime;
  _posArrive = pArrive;
  _posOrigin = pOrigin;
  _energy = pEnergy;
  _direction = pDirection;
  _scatter = pScatter;
  _camName = pCamName;
  _productionVolume = pProductionVolume;
}

OptMenSensorHit::OptMenSensorHit(const OptMenSensorHit& orig) : G4VHit() { *this = orig; }

OptMenSensorHit::~OptMenSensorHit() {}

const OptMenSensorHit& OptMenSensorHit::operator=(const OptMenSensorHit& right) {
  _posArrive = right._posArrive;
  _posOrigin = right._posOrigin;
  _arrivalTime = right._arrivalTime;
  _energy = right._energy;
  _direction = right._direction;
  _scatter = right._scatter;
  _camName = right._camName;
  _productionVolume = right._productionVolume;
  return *this;
}

G4int OptMenSensorHit::operator==(const OptMenSensorHit& right) const {
  return (_posArrive == right._posArrive && _posOrigin == right._posOrigin &&
          _arrivalTime == right._arrivalTime && _energy == right._energy && 
          _direction == right._direction && _scatter == right._scatter && 
          _camName == right._camName && _productionVolume == right._productionVolume);
}
