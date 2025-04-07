// --------------------------------------------------------------------------//

#ifndef _OPTMENCosmicMuonGenerator_HH
#define _OPTMENCosmicMuonGenerator_HH

//---------------------------------------------------------------------------//
#include "OptMenVGenerator.hh"
#include "G4Event.hh"
#include "G4ParticleGun.hh"
#include "G4Box.hh"
#include "globals.hh"
#include "TH1D.h"

//---------------------------------------------------------------------------//
// This generator shoots particles with a G4GeneralParticleSource.
// The source parameters are set via .mac file using the standard G4 messenger
//

class OptMenCosmicMuonGenerator : public OptMenVGenerator {
	public:

		///default constructor
		OptMenCosmicMuonGenerator();
		///destructor
		virtual ~OptMenCosmicMuonGenerator();
		///public interface
		virtual void GeneratePrimaries(G4Event *event);

		G4ParticleGun* GetCosmicMuonGenerator() {return fParticleGun;}
  
    		//Set methods
		void SetRandomFlag(G4bool );
		

	private:
	
	static int eventIndex;

	G4ParticleGun *fParticleGun;

	TH1D* angSpectrum;
	G4Box* fVol;

	G4String fFileName;
	int startingEntry;

};
#endif
