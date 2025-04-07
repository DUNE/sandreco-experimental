// --------------------------------------------------------------------------//

#ifndef _OPTMENPARTICLEGUN_HH
#define _OPTMENPARTTICLEGUN_HH

//---------------------------------------------------------------------------//
#include "OptMenVGenerator.hh"
#include "G4Event.hh"
#include "G4GeneralParticleSource.hh"
#include "globals.hh"

//---------------------------------------------------------------------------//
// This generator shoots particles with a G4GeneralParticleSource.
// The source parameters are set via .mac file using the standard G4 messenger
//

class OptMenParticleGun : public OptMenVGenerator {
	public:

		///default constructor
		OptMenParticleGun();
		///destructor
		virtual ~OptMenParticleGun();
		///public interface
		virtual void GeneratePrimaries(G4Event *event);

		G4GeneralParticleSource* GetParticleGun() {return fParticleSource;}
  
    		//Set methods
		void SetRandomFlag(G4bool );
		

	private:

	G4GeneralParticleSource *fParticleSource;

};
#endif
