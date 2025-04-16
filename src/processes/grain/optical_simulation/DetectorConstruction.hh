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
/// \file persistency/gdml/G04/include/DetectorConstruction.hh
/// \brief Definition of the DetectorConstruction class
//
//
//
//

#ifndef _DetectorConstruction_H_
#define _DetectorConstruction_H_

#include "G4VUserDetectorConstruction.hh"
#include "G4SDManager.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4LogicalSkinSurface.hh" 
#include "G4SurfaceProperty.hh"


class G4GDMLParser;

namespace sand::grain {
class optical_simulation;

struct logicalVolumeStruct {
  G4LogicalVolume* _logicalVolume;
  G4SurfaceProperty* _skinProp;
  G4String collectionName;
  int nSensors = 0;
  std::string name;
};

/// Detector construction for laoding GDML geometry

class DetectorConstruction : public G4VUserDetectorConstruction
{
  public: 
    DetectorConstruction(const G4GDMLParser& parser,  const optical_simulation* optmen_edepsim);

    virtual G4VPhysicalVolume *Construct();  
    virtual void ConstructSDandField();
    virtual G4LogicalVolume *findLogicalDetector(G4LogicalVolume *l, std::string name);

  private:
    const G4GDMLParser& fParser;
    G4int NSiPMs;

    std::map<G4String, logicalVolumeStruct> logicalVolumesMap;

    G4LogicalVolumeStore *lstore;
    const optical_simulation* m_optmen_edepsim; 

};
}
#endif