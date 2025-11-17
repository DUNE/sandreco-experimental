#include <ufw/utils.hpp>
#include <ufw/context.hpp>
#include <grain/photons.h>
#include <geoinfo/grain_info.hpp>

#include <optical_simulation.hpp>

#include <G4Event.hh>
#include <G4HCofThisEvent.hh>
#include <G4SDManager.hh>
#include <G4THitsCollection.hh>
#include "AnalysisManager.hh"

namespace sand::grain {

AnalysisManager::AnalysisManager(optical_simulation* optmen_edepsim) : m_optmen_edepsim(optmen_edepsim) {}

AnalysisManager::~AnalysisManager() {}

void AnalysisManager::BeginOfRun() {
  UFW_DEBUG("Begin of run");
  if (sensorCollID.size() == 0) {
    //nicko: What is the purpose of this??
    G4SDManager* pSDManager = G4SDManager::GetSDMpointer();
    _nCollections = pSDManager->GetCollectionCapacity();
    UFW_DEBUG("There are {} _nCollections.", _nCollections);
  }
}

void AnalysisManager::EndOfRun() {
  UFW_DEBUG("End of run");
}

void AnalysisManager::BeginOfEvent(const G4Event* pEvent) {
  UFW_DEBUG("AnalysisManager::BeginOfEvent {}", pEvent->GetEventID() );
}

void AnalysisManager::EndOfEvent(const G4Event* pEvent) {
  UFW_DEBUG("AnalysisManager::EndOfEvent {}", pEvent->GetEventID() );
  G4HCofThisEvent* pHCofThisEvent = pEvent->GetHCofThisEvent();
  int eventID = pEvent->GetEventID();
  G4SDManager* pSDManager = G4SDManager::GetSDMpointer();
  auto& hits = m_optmen_edepsim->set<sand::grain::hits>("hits");
  const auto& geom = m_optmen_edepsim->instance<geoinfo>();
  geom.grain().lens_cameras();
  geom.grain().mask_cameras();
  //nicko: why starting from 2?
  for (int i = 2; i < _nCollections; i++) {
    SensorHit* sensorHit = nullptr;
    SensorHitCollection* sensorHitsCollection = static_cast<SensorHitCollection*>(pHCofThisEvent->GetHC(i));
    G4int totEntriesScint = sensorHitsCollection->entries();
    UFW_DEBUG("Collection {} ('{}') has {} entries.", i, sensorHitsCollection->GetName(), totEntriesScint);
    if (totEntriesScint != 0) {
      for (int j = 0; j < totEntriesScint; j++) {
        sensorHit = (*sensorHitsCollection)[j];
        sand::grain::hits::photon ph;
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
        ph.inside_camera = (sensorHit->productionVolume() == sensorHit->camName());
        ph.camera_id = geom.grain().at(sensorHit->camName()).id;
        hits.photons.push_back(ph);
      }
    }
  }
}

}
