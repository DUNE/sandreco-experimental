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
/// \file OptMenPrimaryGeneratorAction.hh
/// \brief Definition of the OptMenPrimaryGeneratorAction class

#ifndef OptMenPrimaryGeneratorAction_h
#define OptMenPrimaryGeneratorAction_h 1

#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4GeneralParticleSource.hh"
#include "globals.hh"

#include "G4ThreeVector.hh"
#include "G4Event.hh"
#include "G4String.hh"
#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4NistManager.hh"

#include "TH1D.h"
#include "TLorentzVector.h"
#include "TGeoManager.h"

#include "Randomize.hh"
#include <vector>

#include <TG4Event.h>
#include <TG4HitSegment.h>

class G4Event;
class G4_optmen_edepsim;

/// The primary generator action class is used to switch between generators
/// Instead of implmenting the generator, it builds a generic one (OptMenVGenerator)
/// which is the base class of all implementations (its methods are overloaded)

class OptMenPrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction
{
  public:
    OptMenPrimaryGeneratorAction(const G4_optmen_edepsim* optmen_edepsim);
    virtual ~OptMenPrimaryGeneratorAction();

   ///public interface
		void GeneratePrimaries(G4Event *event) override;


		///reads from root file
		void ReadEDepSimEvent();
        	//Get entry from EDepsim file
		void GetEntry();

		///apply translation to each step
		void ApplyTranslation();
		///returns true if outside LAr volume
		bool ApplyVolumeCut(G4ThreeVector pos);

		// Get lAr material info
		void getMaterialProperties();
		
		//function for random generation
		std::pair<G4ThreeVector,G4ThreeVector> GenerateRandomMomentumPolarization();

    		//functions for fast/slow component
    		G4double GetSingletTripletRatio(double myZ, double myDepEne, double VertexKinEne);
    		G4double getERf90 (double ene);
    		G4double GetLArNuclearQuenching(double myene);

		// Clear vectors
		void clear();

	
	private:
		int startingEntry;
		bool found;

		//OptMenPhotonGeneratorMessenger*   fTheMessenger;
		G4ParticleGun                fParticleGun;
		G4ParticleTable*             fParticleTable;

	
    // lAr info
		TH1D* fastComponentHisto;
		TH1D* slowComponentHisto;
		double fTauFast;
	        double fTauSlow;
                G4double fScintillationYield;

		//////////////////////////////////////////////////////////////
		// Declare the information to get from the EDepSim tree
		//////////////////////////////////////////////////////////////

		// Local coordinates of the lAr volume.
		double local[3] = {0, 0, 0};

		// Global coordinates of the lAr volume.
		double master[3] = {0, 0, 0};

		///Number of hits
		G4int fNHits;
		///Detector volume name
		G4String fDetName; 
		
		///id primary track
		std::vector<int> fPrimaryID;
		std::map<int, int> fPrimaryPDG;
                std::map<int, double> fInitialEnergy;

		///id track of contributors
		std::map<int,std::vector<int>> fContribID;

		///start/stop 4-vectors
		std::vector<TLorentzVector> fStart;
		std::vector<TLorentzVector> fStop;
		std::vector<G4ThreeVector> fStartTranslated;
		std::vector<G4ThreeVector> fStopTranslated;
		
		///step Length
		std::vector<double> fStepLength;

		///energy loss
		std::vector<double> fEnDep;
		std::vector<double> fSecondaryEnDep;
		double fTotEnDep;
		double fTotSecondaryEnDep;
		double tmpEnDep;
		double tmpSecondaryEnDep;

		int currentHit = 0;
		int subEventNumber = 0;

		const G4_optmen_edepsim* m_optmen_edepsim;

		// EDEPTree::const_iterator tree_it;
		// std::vector<EDEPHit>::const_iterator hits_it;
		// bool first_iteration = true;

};
#endif
