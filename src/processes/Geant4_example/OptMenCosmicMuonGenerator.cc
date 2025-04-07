#include "OptMenCosmicMuonGenerator.hh"
#include "OptMenReadParameters.hh"

#include "G4Event.hh"
#include "G4ParticleTable.hh"
#include "G4ParticleDefinition.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4SystemOfUnits.hh"


#include "Randomize.hh"
#include "TSystem.h"

#include <fstream>
#include <limits>

G4Mutex fmutex = G4MUTEX_INITIALIZER; 

//---------------------------------------------------------------------------//

OptMenCosmicMuonGenerator::OptMenCosmicMuonGenerator() : OptMenVGenerator("OptMenCosmicMuonGenerator") {
  fVol = nullptr;
	G4int nofParticles = 1;
	fParticleGun = new G4ParticleGun();
	angSpectrum = new TH1D("Angular Spectrum", "Angular Spectrum", 181, 0, 180);
	for (int i = 0; i < 181; i++) angSpectrum->SetBinContent(i + 1, 1 - cos(i * M_PI / 180) * cos(i * M_PI / 180));
	
	// default particle kinematic
	G4ParticleDefinition* particleDefinition 
		= G4ParticleTable::GetParticleTable()->FindParticle("mu-");

  fFileName = OptMenReadParameters::Get()->GetInputFile();
  startingEntry = OptMenReadParameters::Get()->GetStartingEntry();

	fParticleGun->SetNumberOfParticles(nofParticles);
	fParticleGun->SetParticleDefinition(particleDefinition);
	fParticleGun->SetParticlePosition(G4ThreeVector(0.,0.,0.));
}

int OptMenCosmicMuonGenerator::eventIndex = 1;

//---------------------------------------------------------------------------//

OptMenCosmicMuonGenerator::~OptMenCosmicMuonGenerator()
{
	delete fParticleGun;
}

//-------------------------------------------------------------------------//

void OptMenCosmicMuonGenerator::GeneratePrimaries(G4Event *event) {
	//G4AutoLock l(&fmutex);
	
  G4String cosmicGenerator = "file";
  
  if (cosmicGenerator == "random") {
    bool isContained = false;

    G4double px;
    G4double py;
    G4double pz;
    double energy;
    G4double vtx_x = 0;
    G4double vtx_y = 0;
    G4double vtx_z = 0;
    while(!isContained) {
    
      // Momentum
    	G4double theta = M_PI * G4UniformRand();
    	G4double cost = cos(theta);
      G4double sint = sqrt( 1 - cost*cost );

      G4double phi  = -angSpectrum->GetRandom() * M_PI / 180;
      G4double cosp = cos( phi );
      G4double sinp = sin( phi );

      px = sint*cosp;
      py = sint*sinp;
      pz = cost;

      G4ThreeVector myPhotonMomentum ( px, py, pz );
      fParticleGun->SetParticleMomentumDirection(myPhotonMomentum);

      // Energy
    	energy = G4RandGauss::shoot(4, sqrt(4));
    	fParticleGun->SetParticleEnergy(energy * CLHEP::GeV);

      // Position


    	if (!fVol) {
        G4LogicalVolume* vol = G4LogicalVolumeStore::GetInstance()->GetVolume("lar_volume");
        if ( vol ) fVol = dynamic_cast<G4Box*>(vol->GetSolid());
        std::cout << fVol->GetXHalfLength() << " "
                  << fVol->GetYHalfLength() << " "
                  << fVol->GetZHalfLength() << std::endl;
      }

      if ( fVol ) {
        vtx_x = fVol->GetXHalfLength() * (2.*G4UniformRand() - 1);
        vtx_y = fVol->GetYHalfLength();
        vtx_z = fVol->GetZHalfLength() * (2.*G4UniformRand() - 1);
      } else  {
        G4ExceptionDescription msg;
        msg << "LAr volume of box shape not found.\n"; 
        msg << "Perhaps you have changed geometry.\n";
        msg << "The gun will be place at the center.";
        G4Exception("LCPrimaryGeneratorAction::GeneratePrimaries()",
        "MyCode0002",JustWarning,msg);
      }

    	fParticleGun->SetParticlePosition(G4ThreeVector(vtx_x, vtx_y, vtx_z));

      // Final position

      double finalX = ((-2*fVol->GetYHalfLength()) + tan(phi) * vtx_x) / tan(phi);
      double finalZ = ((-2*fVol->GetYHalfLength()) + tan(-theta) * vtx_z) / tan(-theta);

      // Check if contained
      if (abs(finalX) < fVol->GetXHalfLength() && abs(finalZ) < fVol->GetZHalfLength() && energy > 0)
        isContained = true;

    }

	  fParticleGun->GeneratePrimaryVertex(event);
  }
	
  if (cosmicGenerator == "file") {
    std::ifstream file("/storage/gpfs_data/neutrino/SAND-LAr/SAND-LAr-BIN/OPTICALSIM-BIN/random/muons_list.txt");
    file.seekg(std::ios::beg);
    for(int i=0; i < eventIndex - 1; ++i){
      file.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
    }

    G4double px;
    G4double py;
    G4double pz;
    double energy;
    G4double vtx_x;
    G4double vtx_y;
    G4double vtx_z;

    if (file) {
      file >>  px >> py >> pz >> energy >> vtx_x >> vtx_y >> vtx_z;
      // std::cout << "Event: " << eventIndex << std::endl;
      // std::cout << px << " " << py << " " << pz << " " << energy << " " << vtx_x << " " << vtx_y << " " << vtx_z << std::endl;
      
      event->SetEventID(eventIndex - 1);
      fParticleGun->SetParticlePosition(G4ThreeVector(vtx_x, vtx_y, vtx_z));
      fParticleGun->SetParticleEnergy(energy * CLHEP::GeV);
      G4ThreeVector myPhotonMomentum ( px, py, pz );
      fParticleGun->SetParticleMomentumDirection(myPhotonMomentum);
      fParticleGun->GeneratePrimaryVertex(event);
      eventIndex++;
    } else {
      std::cout << "ERROR : muons_list file not found!"<< std::endl;
		  exit(EXIT_FAILURE);
    }


  }


	G4cout << "<----- INPUT PARTICLE SOURCE ----->" << G4endl;
	G4cout << "Particle: " << fParticleGun->GetParticleDefinition()->GetParticleName() << G4endl;
	G4cout << "Position: " << fParticleGun->GetParticlePosition() << " mm" << G4endl;
	G4cout << "Direction: " << fParticleGun->GetParticleMomentumDirection() << G4endl;
	G4cout << "Energy: " << fParticleGun->GetParticleEnergy() << " MeV" << G4endl;
	G4cout << "<--------------------------------->" << G4endl;

  // std::ofstream outfile;
  // outfile.open("muons_list.txt", std::ios_base::app);
  // outfile << px << " " << py << " " << pz << " " << energy << " " << vtx_x << " " << vtx_y << " " << vtx_z << std::endl;
  // outfile.close();

	l.unlock();
}
