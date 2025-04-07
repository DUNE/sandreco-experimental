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
/// \file OptMenPrimaryGeneratorAction.cc
/// \brief Implementation of the OptMenPrimaryGeneratorAction class

#include "OptMenPrimaryGeneratorAction.hh"
#include "OptMenVGenerator.hh"
#include "OptMenPhotonGenerator.hh"
#include "OptMenParticleGun.hh"
#include "OptMenCosmicMuonGenerator.hh"
#include "OptMenGenieGenerator.hh"

#include "G4Event.hh"

G4Mutex	amutex = G4MUTEX_INITIALIZER;

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

	OptMenPrimaryGeneratorAction::OptMenPrimaryGeneratorAction()
: G4VUserPrimaryGeneratorAction()
{

	std::string generator = OptMenReadParameters::Get()->GetGeneratorType().c_str();

	if(generator.find("macro") != std::string::npos) {
		fGenerator = new OptMenParticleGun;
	} else if (generator.find("edepsim") != std::string::npos) {
		fGenerator = new OptMenPhotonGenerator;
	} else if (generator.find("cosmic") != std::string::npos) {
		fGenerator = new OptMenCosmicMuonGenerator;
	} else if (generator.find("genie") != std::string::npos) {
		fGenerator = new OptMenGenieGenerator;
	} else {
		std::cout << "Unknown input file." << std::endl;
		exit(EXIT_FAILURE);
	}
}
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

OptMenPrimaryGeneratorAction::~OptMenPrimaryGeneratorAction()
{
	delete fGenerator;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void OptMenPrimaryGeneratorAction::GeneratePrimaries(G4Event* anEvent)
{
	//G4AutoLock l(&amutex);
	// This function is called at the begining of event
	fGenerator->GeneratePrimaries(anEvent); 

}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
