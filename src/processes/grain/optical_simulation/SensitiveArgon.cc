/*
 * File:   SensitiveArgon.cpp
 * Author: pozzato
 *
 * Created on April 1, 2014, 12:14 PM
 */

#include "SensitiveArgon.h"

#include "G4SystemOfUnits.hh"
#include "G4OpBoundaryProcess.hh"
#include "G4ParticleDefinition.hh"
#include "G4ParticleTypes.hh"
#include "G4SDManager.hh"
#include "G4Step.hh"
#include "G4ThreeVector.hh"
#include "G4TouchableHistory.hh"
#include "G4Track.hh"
#include "G4VTouchable.hh"
#include "G4ios.hh"
#include "G4Box.hh"

namespace sand::grain {
SensitiveArgon::SensitiveArgon(const G4String& name, const G4String& hitsCollectionName)
    : G4VSensitiveDetector(name), _argonDetHitCollection(0) {
  G4cout << "_argonDetHitCollection:" << hitsCollectionName << G4endl;
  collectionName.insert(hitsCollectionName);
  nHits = 0;

  SetVerboseLevel(2);
}

SensitiveArgon::~SensitiveArgon() {}

void SensitiveArgon::Initialize(G4HCofThisEvent* hitCollection) {

  _argonDetHitCollection =
      new SensitiveArgonHitCollection(SensitiveDetectorName, collectionName[0]);
  G4int HCID = G4SDManager::GetSDMpointer()->GetCollectionID(collectionName[0]);
  hitCollection->AddHitsCollection(HCID, _argonDetHitCollection);

}

G4bool SensitiveArgon::ProcessHits(G4Step* aStep, G4TouchableHistory*) {
  if (aStep == NULL) return false;
  G4Track* theTrack = aStep->GetTrack();

  // Need to know if this is an optical photon
  if (theTrack->GetDefinition() == G4OpticalPhoton::OpticalPhotonDefinition()) {
    return false;
  }

  G4StepPoint* thePostPoint = aStep->GetPostStepPoint();




  // // Find out information regarding the hit
  G4double depositedEnergy = aStep->GetTotalEnergyDeposit() * MeV;
  
  G4int trackID = theTrack->GetTrackID();

  const G4DynamicParticle* dinParticle = theTrack->GetDynamicParticle();
  G4ParticleDefinition* particle = dinParticle->GetDefinition();
  G4int pdgCode = dinParticle->GetPDGcode();
  G4String particleName = particle->GetParticleName();


  G4ThreeVector hitPosition = thePostPoint->GetPosition();



  bool hitAdded = false;

  // Creating the hit and add it to the collection
  _argonDetHitCollection->insert(new SensitiveArgonHit(trackID, pdgCode, depositedEnergy, hitPosition));
  nHits++;
  hitAdded = true;
  return hitAdded;
}

void SensitiveArgon::EndOfEvent(G4HCofThisEvent*) {}
}