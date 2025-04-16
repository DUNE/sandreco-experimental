#include <ufw/utils.hpp>
#include <ufw/context.hpp>
#include <grain/photons.h>

#include <optical_simulation.hpp>

#include <G4ElementTable.hh>
#include <G4EmCalculator.hh>
#include <G4Event.hh>
#include <G4HCofThisEvent.hh>
#include <G4HadronicProcessStore.hh>
#include <G4Material.hh>
#include <G4NistManager.hh>
#include <G4ParticleTable.hh>
#include <G4Run.hh>
#include <G4SDManager.hh>
#include <G4THitsCollection.hh>
#include <G4Version.hh>
#include <numeric>
#include "Sensor.h"
#include "SensitiveArgon.h"
#include "SensorHit.h"
#include "SensitiveArgonHit.h"
#include "G4RunManager.hh"
#include "AnalysisManager.hh"


#include <TROOT.h>
#include "TSystem.h"


#include <string>
#include <vector>

#include <iostream>
#include <memory>
#include <fstream>
#include <stdio.h>

using std::string;
using std::vector;

namespace sand::grain {
AnalysisManager::AnalysisManager(optical_simulation* optmen_edepsim) : m_optmen_edepsim(optmen_edepsim) {}

AnalysisManager::~AnalysisManager() {}

void AnalysisManager::BeginOfRun() {
  UFW_DEBUG("Begin of run");

  auto& hits = ufw::context::instance<sand::grain::hits>(m_optmen_edepsim->outputVariableName());

  // TODO: use info from grain geomanager
  for (auto p : *G4PhysicalVolumeStore::GetInstance()) {
	  std::string volName = p->GetName();
    if( p->GetName().find("CAM_") != std::string::npos)  {
      hits.cameras.emplace_back(sand::grain::hits::camera{0, p->GetName(), {}});
      UFW_DEBUG("Adding camera {}", p->GetName());
      UFW_DEBUG("Cameras size {}", hits.cameras.size());
    }
  }

  if (sensorCollID.size() == 0) {
	  G4SDManager *pSDManager = G4SDManager::GetSDMpointer();
	  _nCollections = pSDManager->GetCollectionCapacity();
	  std::cout << "_nCollections: " << _nCollections << std::endl;
  }
}

void AnalysisManager::EndOfRun() {
  G4cout << "End of run" << std::endl;
}

void AnalysisManager::BeginOfEvent(const G4Event *pEvent) {
  G4cout << "call to  CALAnalysisManager::BeginOfEvent: " << pEvent->GetEventID() << G4endl;
}

void AnalysisManager::EndOfEvent(const G4Event *pEvent) {
	std::cout << "call to AnalysisManager::EndOfEvent: " << pEvent->GetEventID() << std::endl;
  
  G4HCofThisEvent *pHCofThisEvent = pEvent->GetHCofThisEvent();
  SensorHitCollection *sensorHitsCollection = 0;
  
  int eventID = pEvent->GetEventID();
  
  auto& hits = ufw::context::instance<sand::grain::hits>(m_optmen_edepsim->outputVariableName());

  // Retrieving info of detected photons
  G4SDManager *pSDManager = G4SDManager::GetSDMpointer();
  
  auto sorter = [](const auto& lhs, const auto& rhs) { return lhs.camera_name < rhs.camera_name; };
  std::sort(hits.cameras.begin(), hits.cameras.end(), sorter);

  for (int i = 2; i < _nCollections; i++) {
    
    SensorHit *sensorHit; 
    
    sensorHitsCollection = (SensorHitCollection *)(pHCofThisEvent->GetHC(i));
    G4int totEntriesScint = sensorHitsCollection->entries();
    std::cout << i << " " << totEntriesScint << " " << sensorHitsCollection->GetName() << std::endl;
    
    if (totEntriesScint != 0) {
      for (int j = 0; j < totEntriesScint; j++) {
        sensorHit = (*sensorHitsCollection)[j];
        
        auto camera_it = std::find_if(hits.cameras.begin(), hits.cameras.end(), 
                          [sensorHit](const auto& lhs) { return lhs.camera_name == sensorHit->camName();});
        
        sand::grain::hits::photon ph;
        ph.inside_camera = (sensorHit->productionVolume() == sensorHit->camName());
        ph.p.SetPx(sensorHit->direction().getX());
        ph.p.SetPy(sensorHit->direction().getY());
        ph.p.SetPz(sensorHit->direction().getZ());
        ph.p.SetE(sensorHit->energy());
        ph.origin.SetX(sensorHit->originPos().getX());
        ph.origin.SetY(sensorHit->originPos().getY());
        ph.origin.SetZ(sensorHit->originPos().getZ());
        ph.pos.SetXYZT(sensorHit->arrivalPos().getX(), sensorHit->arrivalPos().getY(),
                        sensorHit->arrivalPos().getZ(), sensorHit->arrivalTime());
        ph.scatter = sensorHit->scatter();
        ph.hit = -1;

        camera_it->photons.push_back(ph);
      }
    }
  }

  G4cout << "End of event" << std::endl;
}
}