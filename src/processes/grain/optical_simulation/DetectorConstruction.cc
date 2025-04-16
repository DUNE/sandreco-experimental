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
//
//
/// \file DetectorConstruction.cc
/// \brief Implementation of the DetectorConstruction class

#include "DetectorConstruction.hh"
#include "Sensor.h"
#include "SensitiveArgon.h"

#include "G4SDManager.hh"
#include "G4NistManager.hh"
#include "G4GDMLParser.hh"

namespace sand::grain {

DetectorConstruction::DetectorConstruction(const G4GDMLParser& parser, const optical_simulation* optmen_edepsim )
 : G4VUserDetectorConstruction(),
   fParser(parser),
   m_optmen_edepsim(optmen_edepsim)
{}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4VPhysicalVolume* DetectorConstruction::Construct()
{
  

  return fParser.GetWorldVolume();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DetectorConstruction::ConstructSDandField()
{
  //G4AutoLock lock(&sensitiveDetMutex);

  G4SDManager *SDman = G4SDManager::GetSDMpointer();
  SDman->SetVerboseLevel(2);


  // Used to generate the GROUPVEL property. Geant generates it from the RINDEX but not when the material is from a gdml file.
  // Using a random value for the gdml property and removing/readding the RINDEX prop seems to work
  // G4NistManager* manager = G4NistManager::Instance();
  // G4Material* mat = manager->FindOrBuildMaterial("G4_lXe");
  // G4MaterialPropertiesTable* matTable = mat->GetMaterialPropertiesTable();
  // G4MaterialPropertyVector* property;
  // property = matTable->GetProperty("RINDEX");
  // matTable->RemoveProperty("RINDEX");
  // matTable->RemoveProperty("GROUPVEL");
  // matTable->AddProperty("RINDEX", property);


  SensitiveArgon* argonSD = new SensitiveArgon("lArSensitive", "lArSensitiveHitCollection");
  SDman->AddNewDetector( argonSD );   
  G4VSensitiveDetector* mydet = SDman->FindSensitiveDetector("lArSensitive");
      
  if (mydet) {
    std::string pdName = "lar_volume";
    G4LogicalVolume* myvol = findLogicalDetector(fParser.GetWorldVolume()->GetLogicalVolume(), pdName);
          
    std::cout << myvol->GetName() << " " << pdName << std::endl;
          
    myvol->SetSensitiveDetector(mydet);
  }

  SensitiveArgon* camera_argonSD = new SensitiveArgon("camera_lArSensitive", "camera_lArSensitiveHitCollection");
  SDman->AddNewDetector( camera_argonSD );   
  G4VSensitiveDetector* camera_mydet = SDman->FindSensitiveDetector("camera_lArSensitive");
      
  if (camera_mydet) {
    G4String pdName = "cam_volume";
    G4LogicalVolume* myvol = findLogicalDetector(fParser.GetWorldVolume()->GetLogicalVolume(), pdName);
          
    std::cout << myvol->GetName() << " " << pdName << std::endl;
          
    if (myvol->GetName() != pdName) {
      std::cout << "Skipping camera sensitive volume" << std::endl;
    } else {
      myvol->SetSensitiveDetector(camera_mydet);
    }
  }


  const G4GDMLAuxMapType* auxmap = fParser.GetAuxMap();
  for(G4GDMLAuxMapType::const_iterator aux=auxmap->begin(); aux!=auxmap->end(); aux++) {
    for (G4GDMLAuxListType::const_iterator auxItem = aux->second.begin();auxItem != aux->second.end(); ++auxItem) {
      if (auxItem->type=="Sensor") {
        
        std::string trackerChamberSDname = auxItem->value;
        std::string trackerChamberHitCollectionName = trackerChamberSDname + "_collection";

        Sensor* aTrackerSD = new Sensor(trackerChamberSDname, trackerChamberHitCollectionName, m_optmen_edepsim);
        SDman->AddNewDetector( aTrackerSD );   
        G4VSensitiveDetector* mydet = SDman->FindSensitiveDetector(trackerChamberSDname);
      
        if (mydet) {
          std::string pdName = aux->first->GetName();
          G4LogicalVolume* myvol = findLogicalDetector(fParser.GetWorldVolume()->GetLogicalVolume(), pdName);
          
          std::cout << myvol->GetName() << " " << pdName << std::endl;
          
          myvol->SetSensitiveDetector(mydet);
        }
      }
    }
  }

}

G4LogicalVolume* DetectorConstruction::findLogicalDetector(G4LogicalVolume *l, std::string name) {
  G4LogicalVolumeStore* store = G4LogicalVolumeStore::GetInstance();
  unsigned int length = store->size();
  for (unsigned int iVol = 0; iVol < length; iVol++) {
    std::string volName = ((*store)[iVol])->GetName();
    if ( volName == name ) {
      l = ((*store)[iVol]);
    }
  }
  return l;
}
}