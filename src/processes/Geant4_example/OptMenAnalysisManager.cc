#include <ufw/utils.hpp>
#include <ufw/context.hpp>
#include <grain/photons.h>

#include <G4_optmen_edepsim.hpp>

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
#include "OptMenSensor.h"
#include "OptMenSensitiveArgon.h"
#include "OptMenSensorHit.h"
#include "OptMenSensitiveArgonHit.h"
#include "G4RunManager.hh"
#include "OptMenAnalysisManager.hh"


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

G4Mutex	beginOfEventMutex = G4MUTEX_INITIALIZER;
G4Mutex	endOfEventMutex = G4MUTEX_INITIALIZER;

OptMenAnalysisManager::OptMenAnalysisManager(G4_optmen_edepsim* optmen_edepsim) : m_optmen_edepsim(optmen_edepsim) {}

OptMenAnalysisManager::~OptMenAnalysisManager() {}

void OptMenAnalysisManager::BeginOfRun() {
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

  // // Primary particles output file
  // if (OptMenReadParameters::Get()->GetPrimariesFile() == true) {
  //   if(generator.find("edepsim") == std::string::npos) {
  //     m_pOutputFilePrimary = new TFile(tmpPrimaryFile, "RECREATE");
  //     UFW_INFO("Crated file {} at {} in {}", "m_pOutputFilePrimary", fmt::ptr(m_pOutputFilePrimary), tmpPrimaryFile);

  //     m_pTreePrimary = new TTree("primaries", "Tree w info");
  //     m_pTreePrimary->Branch("idEvent", &m_pEventDataPrimary->eventID,"idEvent/I");
  //     m_pTreePrimary->Branch("xVertex", &m_pEventDataPrimary->xVertex, "xVertex/D");
  //     m_pTreePrimary->Branch("yVertex", &m_pEventDataPrimary->yVertex, "yVertex/D");
  //     m_pTreePrimary->Branch("zVertex", &m_pEventDataPrimary->zVertex, "zVertex/D");
  //     m_pTreePrimary->Branch("px", &m_pEventDataPrimary->px);
  //     m_pTreePrimary->Branch("py", &m_pEventDataPrimary->py);
  //     m_pTreePrimary->Branch("pz", &m_pEventDataPrimary->pz);
  //     m_pTreePrimary->Branch("energy", &m_pEventDataPrimary->energy);
  //     m_pTreePrimary->Branch("pdg", &m_pEventDataPrimary->pdg);

  //     m_pTreeEnergyDeposits = new TTree("energyDeposits", "Tree w info");
  //     m_pTreeEnergyDeposits->Branch("idEvent", &m_pEventDataArgon->eventID);
  //     m_pTreeEnergyDeposits->Branch("trackID", &m_pEventDataArgon->ID);
  //     m_pTreeEnergyDeposits->Branch("depEnergy", &m_pEventDataArgon->energy);
  //     m_pTreeEnergyDeposits->Branch("pdg", &m_pEventDataArgon->pdg);
  //     m_pTreeEnergyDeposits->Branch("x", &m_pEventDataArgon->x);
  //     m_pTreeEnergyDeposits->Branch("y", &m_pEventDataArgon->y);
  //     m_pTreeEnergyDeposits->Branch("z", &m_pEventDataArgon->z);
  //   }
  // }

  // // Full optical photons info output file
  // if (OptMenReadParameters::Get()->GetOpticalPhotonsFile() == true) {
  //   m_pOutputFileStacking = new TFile(tmpOpticalPhotonsFile, "RECREATE");
  //   UFW_INFO("Crated file {} at {} in {}", "m_pOutputFileStacking", fmt::ptr(m_pOutputFileStacking), tmpOpticalPhotonsFile);

  //   m_pTreeStacking = new TTree("opticalPhotons", "Tree w info");
  //   m_pTreeStacking->Branch("idEvent", &m_pEventDataStacking->eventID);
  //   m_pTreeStacking->Branch("x", &m_pEventDataStacking->x);
  //   m_pTreeStacking->Branch("y", &m_pEventDataStacking->y);
  //   m_pTreeStacking->Branch("z", &m_pEventDataStacking->z);
  //   m_pTreeStacking->Branch("px", &m_pEventDataStacking->px);
  //   m_pTreeStacking->Branch("py", &m_pEventDataStacking->py);
  //   m_pTreeStacking->Branch("pz", &m_pEventDataStacking->pz);
  //   m_pTreeStacking->Branch("energy", &m_pEventDataStacking->energy);
  //   m_pTreeStacking->Branch("time", &m_pEventDataStacking->time);
  // }

  if (sensorCollID.size() == 0) {
	  G4SDManager *pSDManager = G4SDManager::GetSDMpointer();
	  _nCollections = pSDManager->GetCollectionCapacity();
	  std::cout << "_nCollections: " << _nCollections << std::endl;
  }
}

void OptMenAnalysisManager::EndOfRun() {
  G4cout << "End of run" << std::endl;
}

void OptMenAnalysisManager::BeginOfEvent(const G4Event *pEvent) {
  G4cout << "call to  CALAnalysisManager::BeginOfEvent: " << pEvent->GetEventID() << G4endl;
}

void OptMenAnalysisManager::EndOfEvent(const G4Event *pEvent) {
	std::cout << "call to OptMenAnalysisManager::EndOfEvent: " << pEvent->GetEventID() << std::endl;
  
  G4HCofThisEvent *pHCofThisEvent = pEvent->GetHCofThisEvent();
  OptMenSensorHitCollection *sensorHitsCollection = 0;
  
  // int eventID; //use EDepSim EvtId: possibly != from file entry!!
  // if(generator.find("edepsim") != std::string::npos) {
	//   eventID = OptMenReadParameters::Get()->GetEDepSimEvtIdFromEntry(pEvent->GetEventID());
  // }
  // else 	
  int eventID = pEvent->GetEventID();
  
  auto& hits = ufw::context::instance<sand::grain::hits>(m_optmen_edepsim->outputVariableName());

  // Retrieving info of detected photons
  G4SDManager *pSDManager = G4SDManager::GetSDMpointer();
  
  auto sorter = [](const auto& lhs, const auto& rhs) { return lhs.camera_name < rhs.camera_name; };
  std::sort(hits.cameras.begin(), hits.cameras.end(), sorter);

  for (int i = 2; i < _nCollections; i++) {
    
    OptMenSensorHit *sensorHit; 
    
    sensorHitsCollection = (OptMenSensorHitCollection *)(pHCofThisEvent->GetHC(i));
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

  // Retrieving info of primary particles
  // if (OptMenReadParameters::Get()->GetPrimariesFile() == true) {
  //   if(generator.find("edepsim") == std::string::npos) {
  //     m_pEventDataPrimary->eventID = eventID;

  //     G4PrimaryVertex* vtx = pEvent->GetPrimaryVertex();
  //     if (vtx)
  //     {
  // 	    m_pEventDataPrimary->xVertex = vtx->GetX0();
  // 	    m_pEventDataPrimary->yVertex = vtx->GetY0();
  // 	    m_pEventDataPrimary->zVertex = vtx->GetZ0();
  // 	    int nPrimaries = vtx->GetNumberOfParticle();
  //       if (nPrimaries > 100) nPrimaries = 100;
  // 	    for (int i = 0; i < nPrimaries; i++)
  // 	    {
  // 	  	  G4PrimaryParticle *part = vtx->GetPrimary(i);
  //         m_pEventDataPrimary->energy.push_back(part->GetTotalEnergy());
  // 	  	  m_pEventDataPrimary->pdg.push_back(part->GetPDGcode());
  // 	  	  m_pEventDataPrimary->px.push_back(part->GetPx());
  // 	  	  m_pEventDataPrimary->py.push_back(part->GetPy());
  // 	  	  m_pEventDataPrimary->pz.push_back(part->GetPz());
  // 	    }
      
  //       OptMenSensitiveArgonHitCollection *argonHitsCollection = 0;
  //       argonHitsCollection = (OptMenSensitiveArgonHitCollection *)(pHCofThisEvent->GetHC(0));
  //       OptMenSensitiveArgonHit *argonHit; 
  //       G4int totEntriesScint = argonHitsCollection->entries();
  //       std::cout << "0 " << totEntriesScint << " " << argonHitsCollection->GetName() << std::endl;

  //       m_pEventDataArgon->eventID = eventID;
      
  //       if (totEntriesScint != 0) {
  //         for (int j = 0; j < totEntriesScint; j++) {
  //           argonHit = (*argonHitsCollection)[j];
  //           if (argonHit->energy() > 0) {
  //             m_pEventDataArgon->energy.push_back(argonHit->energy());
  //             m_pEventDataArgon->ID.push_back(argonHit->trackID());
  //             m_pEventDataArgon->pdg.push_back(argonHit->pdgCode());
  //             m_pEventDataArgon->x.push_back(argonHit->hitPosition().getX());
  //             m_pEventDataArgon->y.push_back(argonHit->hitPosition().getY());
  //             m_pEventDataArgon->z.push_back(argonHit->hitPosition().getZ());
  //           }
  //         }
  //       }

  //       argonHitsCollection = (OptMenSensitiveArgonHitCollection *)(pHCofThisEvent->GetHC(1));
  //       totEntriesScint = argonHitsCollection->entries();
  //       std::cout << "1 " << totEntriesScint << " " << argonHitsCollection->GetName() << std::endl;

  //       if (totEntriesScint != 0) {
  //         for (int j = 0; j < totEntriesScint; j++) {
  //           argonHit = (*argonHitsCollection)[j];
  //           if (argonHit->energy() > 0) {
  //             m_pEventDataArgon->energy.push_back(argonHit->energy());
  //             m_pEventDataArgon->ID.push_back(argonHit->trackID());
  //             m_pEventDataArgon->pdg.push_back(argonHit->pdgCode());
  //             m_pEventDataArgon->x.push_back(argonHit->hitPosition().getX());
  //             m_pEventDataArgon->y.push_back(argonHit->hitPosition().getY());
  //             m_pEventDataArgon->z.push_back(argonHit->hitPosition().getZ());
  //           }
  //         }
  //       }
  //     }

  //     m_pOutputFilePrimary->cd();
  //     m_pTreePrimary->Fill();
  //     m_pTreeEnergyDeposits->Fill();
  //     m_pEventDataPrimary->Clear();
  //     m_pEventDataArgon->Clear();
  //   }
  // }
  G4cout << "End of event" << std::endl;
}