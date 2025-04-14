/*
 * File:   OptMenSensor.cpp
 * Author: pozzato
 *
 * Created on April 1, 2014, 12:14 PM
 */
#include <G4_optmen_edepsim.hpp>

#include "OptMenSensor.h"


#include "G4OpBoundaryProcess.hh"
#include "G4ParticleDefinition.hh"
#include "G4ParticleTypes.hh"
#include "G4SDManager.hh"
#include "G4Step.hh"
#include "G4ThreeVector.hh"
#include "G4TouchableHistory.hh"
#include "G4Track.hh"
#include "G4VTouchable.hh"
#include "G4Navigator.hh"
#include "G4ios.hh"
#include "G4Box.hh"

OptMenSensor::OptMenSensor(const G4String& name, const G4String& hitsCollectionName, const G4_optmen_edepsim* optmen_edepsim)
    : G4VSensitiveDetector(name), _photonDetHitCollection(0), m_optmen_edepsim(optmen_edepsim) {
  G4cout << "_photonDetHitCollection:" << hitsCollectionName << G4endl;
  collectionName.insert(hitsCollectionName);
  nHits = 0;

  SetVerboseLevel(2);
}

OptMenSensor::~OptMenSensor() {}

void OptMenSensor::Initialize(G4HCofThisEvent* hitCollection) {

  _photonDetHitCollection =
      new OptMenSensorHitCollection(SensitiveDetectorName, collectionName[0]);
  G4int HCID = G4SDManager::GetSDMpointer()->GetCollectionID(collectionName[0]);
  hitCollection->AddHitsCollection(HCID, _photonDetHitCollection);

}

G4bool OptMenSensor::ProcessHits(G4Step* aStep, G4TouchableHistory*) {
  if (aStep == NULL) return false;
  G4Track* theTrack = aStep->GetTrack();

  // Need to know if this is an optical photon
  if (theTrack->GetDefinition() != G4OpticalPhoton::OpticalPhotonDefinition())
    return false;

  // Find out information regarding the hit
  G4StepPoint* thePostPoint = aStep->GetPostStepPoint();

  if (thePostPoint->GetStepStatus() != fGeomBoundary) return false;

  G4OpBoundaryProcess* boundary = 0;

  // find the boundary process only once
  if (!boundary) {
    G4ProcessManager* pm =
        aStep->GetTrack()->GetDefinition()->GetProcessManager();
    G4int nprocesses = pm->GetProcessListLength();
    G4ProcessVector* pv = pm->GetProcessList();
    G4int i;
    for (i = 0; i < nprocesses; i++) {
      if ((*pv)[i]->GetProcessName() == "OpBoundary") {
        boundary = (G4OpBoundaryProcess*)(*pv)[i];
        break;
      }
    }
  }

  G4TouchableHistory* theTouchable =
      (G4TouchableHistory*)(thePostPoint->GetTouchable());
  G4OpBoundaryProcessStatus boundaryStatus = boundary->GetStatus();

  G4Navigator aNavigator;
  aNavigator.SetWorldVolume(theTouchable->GetVolume(theTouchable->GetHistoryDepth()));

  std::string camName = theTouchable->GetVolume(1)->GetName();

  //temporary fix for rare cases in which touchable is CAM_XX_YY and camName becomes "lar_physical"
  if(camName.find("CAM") == std::string::npos) return false;

  G4ThreeVector photonArrive = thePostPoint->GetPosition();
  G4ThreeVector photonPositionOrigin = theTrack->GetVertexPosition();
  G4String productionVolume = aNavigator.LocateGlobalPointAndSetup(photonPositionOrigin)->GetName();
  G4ThreeVector photonDirection = thePostPoint->GetMomentumDirection();
  G4ThreeVector photonDirectionOrigin = theTrack->GetVertexMomentumDirection();
  G4double arrivalTime = theTrack->GetGlobalTime();
  G4double energy = thePostPoint->GetTotalEnergy();
  G4double scatterAngle = photonDirectionOrigin.getX() * photonDirection.getX() +
                          photonDirectionOrigin.getY() * photonDirection.getY() +
                          photonDirectionOrigin.getZ() * photonDirection.getZ();
  G4double finalAngle = sqrt(pow(photonDirection.getX(), 2) + pow(photonDirection.getY(), 2) + pow(photonDirection.getZ(), 2));
  G4double initialAngle = sqrt(pow(photonDirectionOrigin.getX(), 2) + pow(photonDirectionOrigin.getY(), 2) + pow(photonDirectionOrigin.getZ(), 2));

  if (photonDirectionOrigin == photonDirection) {
    scatterAngle = 0;
  } else {
    scatterAngle = acos(scatterAngle/(finalAngle * initialAngle));
  }

  G4ThreeVector emissionPosition = theTrack->GetVertexPosition();

  bool hitAdded = false;
  switch (boundaryStatus) {
    case Absorption:
      break;

    case Detection:

      // Convert the global coordinate for arriving photons into
      // the local coordinate of the detector
      photonArrive =
          theTouchable->GetHistory()->GetTopTransform().TransformPoint(
              photonArrive);

	//kill photons from the back (lens assembly is not sealed)
	if(m_optmen_edepsim->opticsType() != G4_optmen_edepsim::OpticsType::MASK  && 
             ( ((G4Box*)theTouchable->GetSolid())->GetZHalfLength() - photonArrive.z() > 0.01) ){
	      //std::cout << "Killed from the back" << std::endl;	
              break;
	}

      // Creating the hit and add it to the collection
      _photonDetHitCollection->insert(
          new OptMenSensorHit(photonArrive, emissionPosition, photonDirection, arrivalTime, energy, scatterAngle, camName, productionVolume));
      nHits++;
      hitAdded = true;
      break;
    default:

      break;
  }

  return hitAdded;
}

void OptMenSensor::EndOfEvent(G4HCofThisEvent*) {}
