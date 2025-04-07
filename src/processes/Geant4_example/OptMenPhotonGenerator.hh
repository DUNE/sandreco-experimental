// --------------------------------------------------------------------------//

#ifndef _OPTMENPHOTONGENERATOR_HH
#define _OPTMENPHOTONGENERATOR_HH

//---------------------------------------------------------------------------//
#include "OptMenVGenerator.hh"
#include "OptMenReadParameters.hh"

#include "G4ThreeVector.hh"
#include "G4Event.hh"
#include "G4String.hh"
#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4NistManager.hh"

#include "TTree.h"
#include "TFile.h"
#include "TH1D.h"
#include "TLorentzVector.h"
#include "TGeoManager.h"

#include "Randomize.hh"
#include <vector>

#include <TG4Event.h>
#include <TG4HitSegment.h>

//---------------------------------------------------------------------------//
// This generator read the input EDepSim root tree, selecting only
// energy deposits in LAr. 
//
// FIXME: For now, since the geometry is not the same, it also 
// applies a coordinate shift to match the LAr modules in the gdml file.
//
// Photons are shot using a G4ParticleGun.
//

class OptMenPhotonGenerator : public OptMenVGenerator {
	public:

		///default constructor
		OptMenPhotonGenerator();

		///destructor
		virtual ~OptMenPhotonGenerator();

		///public interface
		virtual void GeneratePrimaries(G4Event *event);

		void SetFileName(TString f){ fFileName = f;} 

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
		static int eventIndex;
		int startingEntry;
		bool found;

		//OptMenPhotonGeneratorMessenger*   fTheMessenger;
		G4ParticleGun               fParticleGun;
		G4ParticleTable*             fParticleTable;

		TFile* fInput;        //ROOT file to be read.
		TTree* fEDepSimEvents;    //TTree of steps info
		TG4Event* fEvent;
		G4String fFileName;
	
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

		static int currentHit;
		static int subEventNumber;
};
#endif
