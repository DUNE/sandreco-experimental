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
#include "OptMenEventData.hh"
#include "OptMenReadParameters.hh"
#include "OptMenOrderFile.hh"

#include <TFile.h>
#include <TROOT.h>
#include <TTree.h>
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

OptMenAnalysisManager::OptMenAnalysisManager()
{
  m_pEventDataPrimary  = new OptMenEventData();
  m_pEventDataStacking = new OptMenEventData();
  m_pEventDataArgon    = new OptMenEventData();

  tmpPrimaryFile = "/home/sandreco-experimental/build/tmpPrimary.root";
  tmpOpticalPhotonsFile = "/home/sandreco-experimental/build/tmpOptical.root";
  tmpSensorsFile = "/home/sandreco-experimental/build/tmpSensors.root";

  inputFile = OptMenReadParameters::Get()->GetInputFile().c_str();
  generator = OptMenReadParameters::Get()->GetGeneratorType().c_str();

  std::cout << OptMenReadParameters::Get()->GetOpticalPhotonsName() << std::endl;
}


OptMenAnalysisManager::~OptMenAnalysisManager() {}

void OptMenAnalysisManager::CreateFolders() {
  startingPath = gSystem->pwd();
  std::string s = OptMenReadParameters::Get()->GetDestinationPath();
  std::string delimiter = "/";
  std::string token;
  bool endPath = false;
  
  while (!endPath) {
    if (s.find(delimiter) != std::string::npos) {
      token = s.substr(0, s.find(delimiter));
      s.erase(0, s.find(delimiter) + 1);
      path.push_back(token);
    } else {
      if (s != "") {
        token = s;
        path.push_back(token);
        s.erase();
      }
      endPath = true;
    }
  }


  if (path.at(0) != "." && path.at(0) != "..") {
    path.at(0) = "/" + path.at(0);
    gSystem->MakeDirectory(path.at(0).c_str());
    gSystem->cd(path.at(0).c_str());
    path.erase(path.begin());
  }
  
  for (auto elem:path) {
    gSystem->MakeDirectory(elem.c_str());
    gSystem->cd(elem.c_str());
    std::cout << gSystem->pwd() << std::endl; 
    // std::cout << "Token: " << token << std::endl;
    // std::cout << "Remaining path: " << s << std::endl;
  }
  outputPath = gSystem->pwd();
}

void OptMenAnalysisManager::BeginOfRun() {
  std::cout << "Begin of run" << std::endl;
  CreateFolders();
  // Primary particles output file
  if (OptMenReadParameters::Get()->GetPrimariesFile() == true) {
    if(generator.find("edepsim") == std::string::npos) {
      m_pOutputFilePrimary = new TFile(tmpPrimaryFile, "RECREATE");
      m_pTreePrimary = new TTree("primaries", "Tree w info");
      m_pTreePrimary->Branch("idEvent", &m_pEventDataPrimary->eventID,"idEvent/I");
      m_pTreePrimary->Branch("xVertex", &m_pEventDataPrimary->xVertex, "xVertex/D");
      m_pTreePrimary->Branch("yVertex", &m_pEventDataPrimary->yVertex, "yVertex/D");
      m_pTreePrimary->Branch("zVertex", &m_pEventDataPrimary->zVertex, "zVertex/D");
      m_pTreePrimary->Branch("px", &m_pEventDataPrimary->px);
      m_pTreePrimary->Branch("py", &m_pEventDataPrimary->py);
      m_pTreePrimary->Branch("pz", &m_pEventDataPrimary->pz);
      m_pTreePrimary->Branch("energy", &m_pEventDataPrimary->energy);
      m_pTreePrimary->Branch("pdg", &m_pEventDataPrimary->pdg);

      m_pTreeEnergyDeposits = new TTree("energyDeposits", "Tree w info");
      m_pTreeEnergyDeposits->Branch("idEvent", &m_pEventDataArgon->eventID);
      m_pTreeEnergyDeposits->Branch("trackID", &m_pEventDataArgon->ID);
      m_pTreeEnergyDeposits->Branch("depEnergy", &m_pEventDataArgon->energy);
      m_pTreeEnergyDeposits->Branch("pdg", &m_pEventDataArgon->pdg);
      m_pTreeEnergyDeposits->Branch("x", &m_pEventDataArgon->x);
      m_pTreeEnergyDeposits->Branch("y", &m_pEventDataArgon->y);
      m_pTreeEnergyDeposits->Branch("z", &m_pEventDataArgon->z);
    }
  }

  // Full optical photons info output file
  if (OptMenReadParameters::Get()->GetOpticalPhotonsFile() == true) {
    m_pOutputFileStacking = new TFile(tmpOpticalPhotonsFile, "RECREATE");
    m_pTreeStacking = new TTree("opticalPhotons", "Tree w info");
    m_pTreeStacking->Branch("idEvent", &m_pEventDataStacking->eventID);
    m_pTreeStacking->Branch("x", &m_pEventDataStacking->x);
    m_pTreeStacking->Branch("y", &m_pEventDataStacking->y);
    m_pTreeStacking->Branch("z", &m_pEventDataStacking->z);
    m_pTreeStacking->Branch("px", &m_pEventDataStacking->px);
    m_pTreeStacking->Branch("py", &m_pEventDataStacking->py);
    m_pTreeStacking->Branch("pz", &m_pEventDataStacking->pz);
    m_pTreeStacking->Branch("energy", &m_pEventDataStacking->energy);
    m_pTreeStacking->Branch("time", &m_pEventDataStacking->time);
  }

  // Detected optical photons output file
  if (OptMenReadParameters::Get()->GetSensorsFile() == true) {
    m_pOutputFileSensor = new TFile(tmpSensorsFile, "RECREATE");

    unsigned int length = OptMenReadParameters::Get()->GetSensorsTreeName().size();
    std::vector<G4String> sensorsTreeName = OptMenReadParameters::Get()->GetSensorsTreeName();
  
    m_pEventDataSensor = new OptMenEventData();



    for (unsigned int iVol = 0; iVol < length; iVol++) {
        std::cout << " ===> adding " << sensorsTreeName.at(iVol) << std::endl;
    
        eventDataMap[sensorsTreeName.at(iVol)] = new OptMenEventData();
        sensorsMap[sensorsTreeName.at(iVol)] = new TTree(sensorsTreeName.at(iVol), "Tree w info");
        sensorsMap[sensorsTreeName.at(iVol)]->Branch("idEvent", &eventDataMap[sensorsTreeName.at(iVol)]->eventID);
        sensorsMap[sensorsTreeName.at(iVol)]->Branch("energy", &eventDataMap[sensorsTreeName.at(iVol)]->energy);
        sensorsMap[sensorsTreeName.at(iVol)]->Branch("time", &eventDataMap[sensorsTreeName.at(iVol)]->time);
        sensorsMap[sensorsTreeName.at(iVol)]->Branch("x", &eventDataMap[sensorsTreeName.at(iVol)]->x);
        sensorsMap[sensorsTreeName.at(iVol)]->Branch("y", &eventDataMap[sensorsTreeName.at(iVol)]->y);
        sensorsMap[sensorsTreeName.at(iVol)]->Branch("z", &eventDataMap[sensorsTreeName.at(iVol)]->z);
        sensorsMap[sensorsTreeName.at(iVol)]->Branch("xOrigin", &eventDataMap[sensorsTreeName.at(iVol)]->xOrigin);
        sensorsMap[sensorsTreeName.at(iVol)]->Branch("yOrigin", &eventDataMap[sensorsTreeName.at(iVol)]->yOrigin);
        sensorsMap[sensorsTreeName.at(iVol)]->Branch("zOrigin", &eventDataMap[sensorsTreeName.at(iVol)]->zOrigin);
        sensorsMap[sensorsTreeName.at(iVol)]->Branch("px", &eventDataMap[sensorsTreeName.at(iVol)]->px);
        sensorsMap[sensorsTreeName.at(iVol)]->Branch("py", &eventDataMap[sensorsTreeName.at(iVol)]->py);
        sensorsMap[sensorsTreeName.at(iVol)]->Branch("pz", &eventDataMap[sensorsTreeName.at(iVol)]->pz);
        sensorsMap[sensorsTreeName.at(iVol)]->Branch("scatter", &eventDataMap[sensorsTreeName.at(iVol)]->scatter);
        sensorsMap[sensorsTreeName.at(iVol)]->Branch("innerPhotons", &eventDataMap[sensorsTreeName.at(iVol)]->innerPhotons);
    }
  }


  if (sensorCollID.size() == 0) {
	  G4SDManager *pSDManager = G4SDManager::GetSDMpointer();
	  _nCollections = pSDManager->GetCollectionCapacity();
	  std::cout << "_nCollections: " << _nCollections << std::endl;
  }


  gSystem->cd(startingPath.c_str());
}

void OptMenAnalysisManager::EndOfRun() {
  gSystem->cd(outputPath.c_str());
  
  TNamed n(OptMenReadParameters::Get()->GetGeometryHash(), "commit hash of the geometry used to generate the file"); 
  
  if (OptMenReadParameters::Get()->GetSensorsFile() == true) {
    m_pOutputFileSensor->cd();
    n.Write("commit_hash");
    for (auto elem:sensorsMap) elem.second->Write("", TObject::kOverwrite);
    m_pOutputFileSensor->Close();
  }
  if (OptMenReadParameters::Get()->GetPrimariesFile() == true) {
    if(generator.find("edepsim") == std::string::npos) {
      m_pOutputFilePrimary->cd();
      n.Write("commit_hash");
      m_pTreePrimary->Write("", TObject::kOverwrite);
      m_pTreeEnergyDeposits->Write("", TObject::kOverwrite);
      m_pOutputFilePrimary->Close();
    }
  }
  if (OptMenReadParameters::Get()->GetOpticalPhotonsFile() == true) {
    m_pOutputFileStacking->cd();
    n.Write("commit_hash");
    m_pTreeStacking->Write("", TObject::kOverwrite);
    m_pOutputFileStacking->Close();
  }

  if(generator.find("edepsim") != std::string::npos) {
    OptMenOrderFile _reorderFile;
    if (OptMenReadParameters::Get()->GetSensorsFile() == true && OptMenReadParameters::Get()->GetUI() == false) 
      _reorderFile.reorderFile(tmpSensorsFile, OptMenReadParameters::Get()->GetSensorsName());
    if (OptMenReadParameters::Get()->GetOpticalPhotonsFile() == true && OptMenReadParameters::Get()->GetUI() == false)
      _reorderFile.reorderFile(tmpOpticalPhotonsFile, OptMenReadParameters::Get()->GetOpticalPhotonsName());
    
    if (OptMenReadParameters::Get()->GetTemporaryFiles() == false) {
    if (OptMenReadParameters::Get()->GetOpticalPhotonsFile() == true) std::remove(tmpOpticalPhotonsFile);
    if (OptMenReadParameters::Get()->GetSensorsFile() == true) std::remove(tmpSensorsFile);
  }
  } else {
    std::cout << "Skipping the ordering of the files." << std::endl;
    rename(tmpSensorsFile, OptMenReadParameters::Get()->GetSensorsName());
    rename(tmpOpticalPhotonsFile, OptMenReadParameters::Get()->GetOpticalPhotonsName());
    rename(tmpPrimaryFile, OptMenReadParameters::Get()->GetPrimariesName());
  }

  G4cout << "End of run" << std::endl;
}

void OptMenAnalysisManager::BeginOfEvent(const G4Event *pEvent) {
  G4cout << "call to  CALAnalysisManager::BeginOfEvent: " << pEvent->GetEventID() << G4endl;
}

void OptMenAnalysisManager::EndOfEvent(const G4Event *pEvent) {
  
  //G4AutoLock l(&endOfEventMutex);
  
  gSystem->cd(outputPath.c_str());

	std::cout << "call to OptMenAnalysisManager::EndOfEvent: " << pEvent->GetEventID() << std::endl;
  
  G4HCofThisEvent *pHCofThisEvent = pEvent->GetHCofThisEvent();
  OptMenSensorHitCollection *sensorHitsCollection = 0;
  
  int eventID; //use EDepSim EvtId: possibly != from file entry!!
  if(generator.find("edepsim") != std::string::npos) {
	  eventID = OptMenReadParameters::Get()->GetEDepSimEvtIdFromEntry(pEvent->GetEventID());
  }
  else 	
  eventID = pEvent->GetEventID();

// Retrieving info of detected photons
if (OptMenReadParameters::Get()->GetSensorsFile() == true) {
  
  G4SDManager *pSDManager = G4SDManager::GetSDMpointer();
  m_pOutputFileSensor->cd();
  
  
  for (int i = 2; i < _nCollections; i++) {
    
    OptMenSensorHit *sensorHit; 
    
    sensorHitsCollection = (OptMenSensorHitCollection *)(pHCofThisEvent->GetHC(i));
    G4int totEntriesScint = sensorHitsCollection->entries();
    std::cout << i << " " << totEntriesScint << " " << sensorHitsCollection->GetName() << std::endl;
    
    
    
    for (auto elem:eventDataMap) {
      elem.second->eventID = eventID;
      elem.second->innerPhotons = 0;
    }
    
      if (totEntriesScint != 0) {
        for (int j = 0; j < totEntriesScint; j++) {
          sensorHit = (*sensorHitsCollection)[j];
          if (sensorHit->productionVolume() == sensorHit->camName()) {
            eventDataMap[sensorHit->camName()]->innerPhotons += 1;
          }
          eventDataMap[sensorHit->camName()]->energy.push_back(sensorHit->energy());
          eventDataMap[sensorHit->camName()]->time.push_back(sensorHit->arrivalTime());
          eventDataMap[sensorHit->camName()]->x.push_back(sensorHit->arrivalPos().getX());
          eventDataMap[sensorHit->camName()]->y.push_back(sensorHit->arrivalPos().getY());
          eventDataMap[sensorHit->camName()]->z.push_back(sensorHit->arrivalPos().getZ());
          eventDataMap[sensorHit->camName()]->xOrigin.push_back(sensorHit->originPos().getX());
          eventDataMap[sensorHit->camName()]->yOrigin.push_back(sensorHit->originPos().getY());
          eventDataMap[sensorHit->camName()]->zOrigin.push_back(sensorHit->originPos().getZ());
          eventDataMap[sensorHit->camName()]->px.push_back(sensorHit->direction().getX());
          eventDataMap[sensorHit->camName()]->py.push_back(sensorHit->direction().getY());
          eventDataMap[sensorHit->camName()]->pz.push_back(sensorHit->direction().getZ());
          eventDataMap[sensorHit->camName()]->scatter.push_back(sensorHit->scatter());
        }
      }
      
    }
      for (auto elem:sensorsMap) {
        elem.second->Fill();
         eventDataMap[elem.first]->Clear();
      }
  }
  // Retrieving info of primary particles
  if (OptMenReadParameters::Get()->GetPrimariesFile() == true) {
    if(generator.find("edepsim") == std::string::npos) {
      m_pEventDataPrimary->eventID = eventID;

      G4PrimaryVertex* vtx = pEvent->GetPrimaryVertex();
      if (vtx)
      {
  	    m_pEventDataPrimary->xVertex = vtx->GetX0();
  	    m_pEventDataPrimary->yVertex = vtx->GetY0();
  	    m_pEventDataPrimary->zVertex = vtx->GetZ0();
  	    int nPrimaries = vtx->GetNumberOfParticle();
        if (nPrimaries > 100) nPrimaries = 100;
  	    for (int i = 0; i < nPrimaries; i++)
  	    {
  	  	  G4PrimaryParticle *part = vtx->GetPrimary(i);
          m_pEventDataPrimary->energy.push_back(part->GetTotalEnergy());
  	  	  m_pEventDataPrimary->pdg.push_back(part->GetPDGcode());
  	  	  m_pEventDataPrimary->px.push_back(part->GetPx());
  	  	  m_pEventDataPrimary->py.push_back(part->GetPy());
  	  	  m_pEventDataPrimary->pz.push_back(part->GetPz());
  	    }
      
        OptMenSensitiveArgonHitCollection *argonHitsCollection = 0;
        argonHitsCollection = (OptMenSensitiveArgonHitCollection *)(pHCofThisEvent->GetHC(0));
        OptMenSensitiveArgonHit *argonHit; 
        G4int totEntriesScint = argonHitsCollection->entries();
        std::cout << "0 " << totEntriesScint << " " << argonHitsCollection->GetName() << std::endl;

        m_pEventDataArgon->eventID = eventID;
      
        if (totEntriesScint != 0) {
          for (int j = 0; j < totEntriesScint; j++) {
            argonHit = (*argonHitsCollection)[j];
            if (argonHit->energy() > 0) {
              m_pEventDataArgon->energy.push_back(argonHit->energy());
              m_pEventDataArgon->ID.push_back(argonHit->trackID());
              m_pEventDataArgon->pdg.push_back(argonHit->pdgCode());
              m_pEventDataArgon->x.push_back(argonHit->hitPosition().getX());
              m_pEventDataArgon->y.push_back(argonHit->hitPosition().getY());
              m_pEventDataArgon->z.push_back(argonHit->hitPosition().getZ());
            }
          }
        }

        argonHitsCollection = (OptMenSensitiveArgonHitCollection *)(pHCofThisEvent->GetHC(1));
        totEntriesScint = argonHitsCollection->entries();
        std::cout << "1 " << totEntriesScint << " " << argonHitsCollection->GetName() << std::endl;

        if (totEntriesScint != 0) {
          for (int j = 0; j < totEntriesScint; j++) {
            argonHit = (*argonHitsCollection)[j];
            if (argonHit->energy() > 0) {
              m_pEventDataArgon->energy.push_back(argonHit->energy());
              m_pEventDataArgon->ID.push_back(argonHit->trackID());
              m_pEventDataArgon->pdg.push_back(argonHit->pdgCode());
              m_pEventDataArgon->x.push_back(argonHit->hitPosition().getX());
              m_pEventDataArgon->y.push_back(argonHit->hitPosition().getY());
              m_pEventDataArgon->z.push_back(argonHit->hitPosition().getZ());
            }
          }
        }
      }

      m_pOutputFilePrimary->cd();
      m_pTreePrimary->Fill();
      m_pTreeEnergyDeposits->Fill();
      m_pEventDataPrimary->Clear();
      m_pEventDataArgon->Clear();
    }
  }
  
  G4cout << "End of event" << std::endl;
  gSystem->cd(startingPath.c_str());
}

void OptMenAnalysisManager::NewStage(OptMenEventData* sData) {
  m_pEventDataStacking->eventID = sData->eventID;
  
  for (unsigned int i = 0; i < sData->x.size(); i++) {
    
    m_pEventDataStacking->x.push_back(sData->x.at(i));
    m_pEventDataStacking->y.push_back(sData->y.at(i));
    m_pEventDataStacking->z.push_back(sData->z.at(i));

    m_pEventDataStacking->px.push_back(sData->px.at(i));
    m_pEventDataStacking->py.push_back(sData->py.at(i));
    m_pEventDataStacking->pz.push_back(sData->pz.at(i));

    m_pEventDataStacking->energy.push_back(sData->energy.at(i));
    m_pEventDataStacking->time.push_back(sData->time.at(i));
  }

  m_pOutputFileStacking->cd();
  m_pTreeStacking->Fill();
  m_pEventDataStacking->Clear();
}
