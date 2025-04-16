#ifndef __CALAALYSISMANAGER_H__
#define __CALAALYSISMANAGER_H__

#include <globals.hh>
#include "SensorHit.h"
#include "Sensor.h"
#include "G4GDMLParser.hh"
#include "G4PhysicalVolumeStore.hh"

class G4Run;
class G4Event;
class G4Step;

class CALPrimaryGeneratorAction;

class G4_optmen_edepsim;

class AnalysisManager
{
public:

  AnalysisManager(G4_optmen_edepsim* optmen_edepsim);
  virtual ~AnalysisManager();
  int _nCollections;

public:
  virtual void BeginOfRun(); 
  virtual void EndOfRun(); 
  virtual void BeginOfEvent(const G4Event *pEvent); 
  virtual void EndOfEvent(const G4Event *pEvent);

  G4_optmen_edepsim* m_optmen_edepsim;
private:
  SensorHitCollection* GetHitsCollection(const G4String& hcName,const G4Event* event) const;
  
  std::vector<G4int> sensorCollID;


  
};

#endif // __ANALYSISMANAGER_H__

