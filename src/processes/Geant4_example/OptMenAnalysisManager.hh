#ifndef __CALAALYSISMANAGER_H__
#define __CALAALYSISMANAGER_H__

#include <globals.hh>
#include "OptMenSensorHit.h"
#include "OptMenSensor.h"
#include "G4GDMLParser.hh"
#include "G4PhysicalVolumeStore.hh"

class G4Run;
class G4Event;
class G4Step;

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
  virtual void NewStage();

private:
  OptMenSensorHitCollection* GetHitsCollection(const G4String& hcName,const G4Event* event) const;
  
  std::vector<G4int> sensorCollID;

  std::string inputFile;
  std::string generator;

  
};

#endif // __OptMenANALYSISMANAGER_H__

