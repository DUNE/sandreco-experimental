#ifndef __CALAALYSISMANAGER_H__
#define __CALAALYSISMANAGER_H__

#include <globals.hh>
#include "OptMenSensorHit.h"
#include "OptMenSensor.h"
#include "OptMenEventData.hh"
#include "G4GDMLParser.hh"
#include "G4PhysicalVolumeStore.hh"

class G4Run;
class G4Event;
class G4Step;

class TFile;
class TTree;
class CALPrimaryGeneratorAction;


class OptMenAnalysisManager
{
public:

  OptMenAnalysisManager();
  virtual ~OptMenAnalysisManager();
  int _nCollections;

public:
  virtual void BeginOfRun(); 
  virtual void EndOfRun(); 
  virtual void BeginOfEvent(const G4Event *pEvent); 
  virtual void EndOfEvent(const G4Event *pEvent);
  virtual void NewStage(OptMenEventData* sData);
  virtual void CreateFolders();
  
private:
  OptMenSensorHitCollection* GetHitsCollection(const G4String& hcName,const G4Event* event) const;
  
  std::vector<G4int> sensorCollID;

  std::string inputFile;
  std::string generator;

  OptMenEventData* m_pEventDataSensor;
  OptMenEventData* m_pEventDataStacking;
  OptMenEventData *m_pEventDataPrimary;
  OptMenEventData *m_pEventDataArgon;
  
  TFile *m_pOutputFilePrimary;
  TFile *m_pOutputFileSensor;
  TFile *m_pOutputFileStacking;

  G4String tmpPrimaryFile;
  G4String tmpOpticalPhotonsFile;
  G4String tmpSensorsFile;
  
  std::vector<TTree*> m_pTreeSensor;
  TTree *m_pTreePrimary;
  TTree *m_pTreeStacking;
  TTree *m_pTreeEnergyDeposits;

  std::map<G4String, TTree*> sensorsMap;
  std::map<G4String, OptMenEventData*> eventDataMap;

  std::vector<std::string> path;
  std::string startingPath;
  std::string outputPath;
  
};

#endif // __OptMenANALYSISMANAGER_H__

