#include "OptMenParticleGun.hh"

#include "G4Event.hh"
#include "G4ParticleTable.hh"
#include "G4ParticleDefinition.hh"
#include "G4SystemOfUnits.hh"

#include "Randomize.hh"

G4Mutex mutex = G4MUTEX_INITIALIZER; 

//---------------------------------------------------------------------------//

OptMenParticleGun::OptMenParticleGun() : OptMenVGenerator("OptMenParticleGun") {

	G4int nofParticles = 1;
	fParticleSource = new G4GeneralParticleSource();
	
	// default particle kinematic
	G4ParticleDefinition* particleDefinition 
		= G4ParticleTable::GetParticleTable()->FindParticle("proton");

	fParticleSource->SetNumberOfParticles(nofParticles);
	fParticleSource->SetParticleDefinition(particleDefinition);
	fParticleSource->SetParticlePosition(G4ThreeVector(0.,0.,0.));
}

//---------------------------------------------------------------------------//

OptMenParticleGun::~OptMenParticleGun()
{
	delete fParticleSource;
}

//-------------------------------------------------------------------------//

void OptMenParticleGun::GeneratePrimaries(G4Event *event) {

	// The G4GeneralParticleSource parameters are set by the .mac file
	// using the standard G4GeneralParticleSourceMessenger commands

	//G4AutoLock l(&mutex);
	
	fParticleSource->GeneratePrimaryVertex(event);
	
	G4cout << "<----- INPUT PARTICLE SOURCE ----->" << G4endl;
	G4cout << "Particle: " << fParticleSource->GetParticleDefinition()->GetParticleName() << G4endl;
	G4cout << "Position: " << fParticleSource->GetParticlePosition() << " mm" << G4endl;
	G4cout << "Direction: " << fParticleSource->GetParticleMomentumDirection() << G4endl;
	G4cout << "Energy: " << fParticleSource->GetParticleEnergy() << " MeV" << G4endl;
	G4cout << "<--------------------------------->" << G4endl;
			
}
