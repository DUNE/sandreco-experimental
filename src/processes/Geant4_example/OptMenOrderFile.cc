#include "OptMenOrderFile.hh"
#include "OptMenEventData.hh"
#include "OptMenReadParameters.hh"

#include "TEntryList.h"
#include "TCutG.h"

#include <string>

OptMenOrderFile::OptMenOrderFile()
{}

OptMenOrderFile::~OptMenOrderFile() 
{}

void OptMenOrderFile::reorderFile(G4String tmpName, G4String outName) {
  m_pInputFile = new TFile(tmpName, "UPDATE");
  m_pOutputFile = new TFile(outName, "RECREATE");
  TIter next(m_pInputFile->GetListOfKeys());
  
  while ((key=(TKey*)next())) {
    if (!strcmp(key->GetClassName(),"TNamed")) {
      std::cout << "Copying hash info" << std::endl;
      m_pInputFile->GetObject(key->GetName(),commit_hash);
      commit_hash->Write("commit_hash");
      continue;
    }
    std::cout << "Ordering tree " << key->GetName() << std::endl;
    inizializeInputTree();
    if (outName == OptMenReadParameters::Get()->GetOpticalPhotonsName())
      outputTree = inizializeOutputOpticalTree();
    if (outName == OptMenReadParameters::Get()->GetSensorsName())         
      outputTree = inizializeOutputSensorsTree();
    

    int entries = inputTree->GetEntries();
    int startingEntry = OptMenReadParameters::Get()->GetStartingEntry();
    int requestedEntry = OptMenReadParameters::Get()->GetStartingEntry();
 
    int requestedID = OptMenReadParameters::Get()->GetEDepSimEvtIdFromEntry(requestedEntry);
    
    int entryIndex = 0;
    addFirstElement();

    while (true) {
      if (entryIndex == entries) {
        entryIndex = 0;
        std::cout << "End of file reached. Missing events, restarting from entry 0." << std::endl;
      }
      inputTree->GetEntry(entryIndex);
      std::cout << "Looking for event " << requestedID << std::endl;
      if (eventID == requestedID) {
        std::cout << "Event found at entry " << entryIndex << std::endl;
        insertVectors();
        eventIDOrdered = eventID;
        std::cout << "Searching other events with same ID." << std::endl;
        TEntryList* elist;
        inputTree->Draw(">>elist", Form("idEvent==%d",eventIDOrdered), "entrylist");
        elist = (TEntryList*)gDirectory->Get("elist");
        for (int j = 0; j < elist->GetN(); j++) {
          if (elist->GetEntry(j) != entryIndex) {
            std::cout << "Found same ID at entry " << elist->GetEntry(j) << ". Merging.." << std::endl;
            inputTree->GetEntry(elist->GetEntry(j));
            insertVectors();
          }
        }
        eraseFirstElement();
        outputTree->Fill();
        clearVectors();
        std::cout << "Riempito evento " << requestedID << std::endl;
        
      	requestedEntry++;
	      requestedID = OptMenReadParameters::Get()->GetEDepSimEvtIdFromEntry(requestedEntry);

        if (requestedEntry - startingEntry == OptMenReadParameters::Get()->GetEventNumber()) break;
        addFirstElement();
      }
      entryIndex++;
    }
    outputTree->Write("", TObject::kOverwrite);
  }
  m_pOutputFile->Close();
}

void OptMenOrderFile::inizializeInputTree() {
  inputTree = (TTree*) m_pInputFile->Get(key->GetName());
  if (inputTree == 0) 
    return;
  
  string inputFileName = m_pInputFile->GetName();

  inputTree->SetBranchAddress("idEvent", &eventID);
  inputTree->SetBranchAddress("energy", &energy);
  inputTree->SetBranchAddress("px", &px);
  inputTree->SetBranchAddress("py", &py);
  inputTree->SetBranchAddress("pz", &pz);
  inputTree->SetBranchAddress("x", &x);
  inputTree->SetBranchAddress("y", &y);
  inputTree->SetBranchAddress("z", &z);
  inputTree->SetBranchAddress("time", &time);

  if(inputFileName.find("Sensors") != std::string::npos){
    inputTree->SetBranchAddress("xOrigin", &xOrigin);
    inputTree->SetBranchAddress("yOrigin", &yOrigin);
    inputTree->SetBranchAddress("zOrigin", &zOrigin);
    inputTree->SetBranchAddress("scatter", &scatter);
    inputTree->SetBranchAddress("innerPhotons", &innerPhotons);
  }
}

TTree* OptMenOrderFile::inizializeOutputSensorsTree() {
  m_pOutputFile->cd();
  m_pTreeSensor = new TTree(key->GetName(), "Tree w info");
  m_pTreeSensor->Branch("idEvent", &eventIDOrdered);
  m_pTreeSensor->Branch("energy", &energyOrdered);
  m_pTreeSensor->Branch("time", &timeOrdered);
  m_pTreeSensor->Branch("x", &xOrdered);
  m_pTreeSensor->Branch("y", &yOrdered);
  m_pTreeSensor->Branch("z", &zOrdered);
  m_pTreeSensor->Branch("xOrigin", &xOriginOrdered);
  m_pTreeSensor->Branch("yOrigin", &yOriginOrdered);
  m_pTreeSensor->Branch("zOrigin", &zOriginOrdered);
  m_pTreeSensor->Branch("px", &pxOrdered);
  m_pTreeSensor->Branch("py", &pyOrdered);
  m_pTreeSensor->Branch("pz", &pzOrdered);
  m_pTreeSensor->Branch("scatter", &scatterOrdered);
  m_pTreeSensor->Branch("innerPhotons", &innerPhotonsOrdered);

  return m_pTreeSensor;
}

TTree* OptMenOrderFile::inizializeOutputOpticalTree() {
  m_pOutputFile->cd();
  m_pTreeOptical = new TTree(key->GetName(), "Tree w info");
  m_pTreeOptical->Branch("idEvent", &eventIDOrdered);
  m_pTreeOptical->Branch("energy", &energyOrdered);
  m_pTreeOptical->Branch("time", &timeOrdered);
  m_pTreeOptical->Branch("x", &xOrdered);
  m_pTreeOptical->Branch("y", &yOrdered);
  m_pTreeOptical->Branch("z", &zOrdered);
  m_pTreeOptical->Branch("px", &pxOrdered);
  m_pTreeOptical->Branch("py", &pyOrdered);
  m_pTreeOptical->Branch("pz", &pzOrdered);

  return m_pTreeOptical;
}

void OptMenOrderFile::addFirstElement() {
  energyOrdered.push_back(0);
  timeOrdered.push_back(0);
  xOrdered.push_back(0);
  yOrdered.push_back(0);
  zOrdered.push_back(0);
  xOriginOrdered.push_back(0);
  yOriginOrdered.push_back(0);
  zOriginOrdered.push_back(0);
  pxOrdered.push_back(0);
  pyOrdered.push_back(0);
  pzOrdered.push_back(0);
  scatterOrdered.push_back(0);
  innerPhotonsOrdered = 0;
}

void OptMenOrderFile::insertVectors() {
  if (energy) energyOrdered.insert(energyOrdered.end(), energy->begin(), energy->end());
  if (time) timeOrdered.insert(timeOrdered.end(), time->begin(), time->end());
  if (x) xOrdered.insert(xOrdered.end(), x->begin(), x->end());
  if (y) yOrdered.insert(yOrdered.end(), y->begin(), y->end());
  if (z) zOrdered.insert(zOrdered.end(), z->begin(), z->end());
  if (xOrigin) xOriginOrdered.insert(xOriginOrdered.end(), xOrigin->begin(), xOrigin->end());
  if (yOrigin) yOriginOrdered.insert(yOriginOrdered.end(), yOrigin->begin(), yOrigin->end());
  if (zOrigin) zOriginOrdered.insert(zOriginOrdered.end(), zOrigin->begin(), zOrigin->end());
  if (px) pxOrdered.insert(pxOrdered.end(), px->begin(), px->end());
  if (py) pyOrdered.insert(pyOrdered.end(), py->begin(), py->end());
  if (pz) pzOrdered.insert(pzOrdered.end(), pz->begin(), pz->end());
  if (scatter) scatterOrdered.insert(scatterOrdered.end(), scatter->begin(), scatter->end());
  if (innerPhotons) innerPhotonsOrdered += innerPhotons;
}

void OptMenOrderFile::eraseFirstElement() {
  energyOrdered.erase(energyOrdered.begin());
  timeOrdered.erase(timeOrdered.begin());
  xOrdered.erase(xOrdered.begin());
  yOrdered.erase(yOrdered.begin());
  zOrdered.erase(zOrdered.begin());
  xOriginOrdered.erase(xOriginOrdered.begin());
  yOriginOrdered.erase(yOriginOrdered.begin());
  zOriginOrdered.erase(zOriginOrdered.begin());
  pxOrdered.erase(pxOrdered.begin());
  pyOrdered.erase(pyOrdered.begin());
  pzOrdered.erase(pzOrdered.begin());
  scatterOrdered.erase(scatterOrdered.begin());
}

void OptMenOrderFile::clearVectors() {
  energyOrdered.clear();
  timeOrdered.clear();
  xOrdered.clear();
  yOrdered.clear();
  zOrdered.clear();
  xOriginOrdered.clear();
  yOriginOrdered.clear();
  zOriginOrdered.clear();
  pxOrdered.clear();
  pyOrdered.clear();
  pzOrdered.clear();
  scatterOrdered.clear();
  innerPhotonsOrdered = 0;
}
