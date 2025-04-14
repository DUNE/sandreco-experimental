//
// ********************************************************************
// * License and Disclaimer                                           *
// *                                                                  *
// * The  Geant4 software  is  copyright of the Copyright Holders  of *
// * the Geant4 Collaboration.  It is provided  under  the terms  and *
// * conditions of the Geant4 Software License,  included in the file *
// * LICENSE and available at  http://cern.ch/geant4/license .  These *
// * include a list of copyright holders.                             *
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.  Please see the license in the file  LICENSE  and URL above *
// * for the full disclaimer and the limitation of liability.         *
// *                                                                  *
// * This  code  implementation is the result of  the  scientific and *
// * technical work of the GEANT4 collaboration.                      *
// * By using,  copying,  modifying or  distributing the software (or *
// * any work based  on the software)  you  agree  to acknowledge its *
// * use  in  resulting  scientific  publications,  and indicate your *
// * acceptance of all terms of the Geant4 Software license.          *
// ********************************************************************

#include "OptMenStackingAction.hh"

#include "G4VProcess.hh"
#include "G4Track.hh"
#include "G4ParticleDefinition.hh"
#include "G4OpticalPhoton.hh"  
#include "G4ThreeVector.hh"  
#include "G4ios.hh"
#include <G4RunManager.hh>

#include <string.h>

OptMenStackingAction::OptMenStackingAction(OptMenAnalysisManager* mgr)
  :G4UserStackingAction()
{
  _anMgr = mgr;
}

OptMenStackingAction::~OptMenStackingAction() {}

G4ClassificationOfNewTrack OptMenStackingAction::ClassifyNewTrack(const G4Track * aTrack)
{
  // sData->eventID = G4RunManager::GetRunManager()->GetCurrentEvent()->GetEventID();  
  // if(aTrack->GetDefinition() == G4OpticalPhoton::OpticalPhotonDefinition()) {
    
    
  //   sData->energy.push_back(aTrack->GetTotalEnergy());
  //   sData->time.push_back(aTrack->GetGlobalTime());

  //   G4ThreeVector mom = aTrack->GetMomentum();
  //   sData->px.push_back(mom.x());
  //   sData->py.push_back(mom.y());
  //   sData->pz.push_back(mom.z());

  //   G4ThreeVector pos = aTrack->GetPosition();
  //   sData->x.push_back(pos.x());
  //   sData->y.push_back(pos.y());
  //   sData->z.push_back(pos.z());
  // }
  return fUrgent; 
}

void OptMenStackingAction::NewStage()
{
  _anMgr->NewStage();
}

void OptMenStackingAction::PrepareNewEvent() {}

