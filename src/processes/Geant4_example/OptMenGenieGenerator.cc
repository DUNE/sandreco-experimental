// --------------------------------------------------------------------------//
#include <TF1.h>
#include "G4Event.hh"
#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4Electron.hh"
#include "G4IonTable.hh"
#include "OptMenGenieGenerator.hh"
#include "OptMenReadParameters.hh"  
#include "G4SystemOfUnits.hh"
#include <cmath>

G4Mutex tmutex = G4MUTEX_INITIALIZER; 

//---------------------------------------------------------------------------//

OptMenGenieGenerator::OptMenGenieGenerator(): OptMenVGenerator("OptMenGenieGenerator") {

	fParticleGun  = new G4ParticleGun ;
	fParticleTable = G4ParticleTable::GetParticleTable();
	fParticleGun->SetParticleDefinition(G4Electron::Definition()); //FIXME, geantino!!!
	
  startingEntry = OptMenReadParameters::Get()->GetStartingEntry();
  nEvents = OptMenReadParameters::Get()->GetEventNumber();

  fIsFirstTime = true;
	fIsLastTime = false;
	fFileName = OptMenReadParameters::Get()->GetInputFile();
  std::cout << fFileName << std::endl;

	if(!fParticleGun) {
		std::cout << "Could not allocate G4ParticleGun! Out of memory?"<< std::endl;
		std::cout << std::endl;
		exit(EXIT_FAILURE);
	}
	std::cout << "G4ParticleGun Constructed." << std::endl;

}
//---------------------------------------------------------------------------//

OptMenGenieGenerator::~OptMenGenieGenerator()
{
	delete fParticleGun;
	delete fInput;
}

//---------------------------------------------------------------------------//

int OptMenGenieGenerator::eventIndex = 0;
int OptMenGenieGenerator::eventCount = 0;

void OptMenGenieGenerator::GeneratePrimaries(G4Event *event) {
  //G4AutoLock l(&tmutex);

  // Read the tree once from the input file
	if(fIsFirstTime) {
		ReadTree();
	}

  // Get the desired entry
	fTree->GetEntry(startingEntry + eventIndex);

	std::cout << "Checking entry " << startingEntry + eventIndex << std::endl;
	std::cout << "Number of event considered in the Genie Tree "<< fEvtNum << std::endl;
  
  // If GetStartingOnly() == true icrease the eventID progressively
  // starting from startingEntry. This is needed for reorderFile to work later.
  // If GetStartingOnly() == false set the eventID to the one of the 
  // input file.
  if (OptMenReadParameters::Get()->GetStartingOnly() == true)	{
    event->SetEventID(eventCount + startingEntry);
    std::cout <<"Event ID "<< eventCount + startingEntry << std::endl;
  } else {
    event->SetEventID(fEvtNum);
    std::cout <<"Event ID "<< fEvtNum << std::endl;
  }

  

  //assegna il numero della traccia (lo stesso che genera geant)
	int countTrack = 0; 
  
  // Default vertex position
  G4double vtx_x = 0;
  G4double vtx_y = 0;
  G4double vtx_z = 0;

  // Get the lar volume info
  if (!fVol) {
    G4LogicalVolume* vol = G4LogicalVolumeStore::GetInstance()->GetVolume("lar_volume");
    if ( vol ) fVol = dynamic_cast<G4Box*>(vol->GetSolid());
    std::cout << fVol->GetXHalfLength() << " "
              << fVol->GetYHalfLength() << " "
              << fVol->GetZHalfLength() << std::endl;
  }

  // If GetFullVolume() == true generate events uniformly in the lar volume
  // If GetFullVolume() == false generate events only in 1/24th of the cube (more in readme)
  if ( fVol ) {
    if (OptMenReadParameters::Get()->GetFullVolume() == true) {
      vtx_x = fVol->GetXHalfLength() * (2.*G4UniformRand() - 1);
      vtx_y = fVol->GetYHalfLength() * (2.*G4UniformRand() - 1);
      vtx_z = fVol->GetZHalfLength() * (2.*G4UniformRand() - 1);
    } else {
      vtx_z = fVol->GetZHalfLength() * (G4UniformRand() - 1);
      vtx_y = fVol->GetYHalfLength() * G4UniformRand();
      vtx_x = fVol->GetXHalfLength() * G4UniformRand();

	    while (vtx_x > -vtx_z || vtx_y > -vtx_z) {
        vtx_z = fVol->GetZHalfLength() * (G4UniformRand() - 1);
        vtx_y = fVol->GetYHalfLength() * G4UniformRand();
        vtx_x = fVol->GetXHalfLength() * G4UniformRand();
      }
    }    
  } else  {
    G4ExceptionDescription msg;
    msg << "LAr volume of box shape not found.\n"; 
    msg << "Perhaps you have changed geometry.\n";
    msg << "The gun will be place at the center.";
    G4Exception("LCPrimaryGeneratorAction::GeneratePrimaries()",
    "MyCode0002",JustWarning,msg);
  }
  std::cout << "Vertex position: " << vtx_x << " " << vtx_y << " " << vtx_z << std::endl;
  
  G4PrimaryVertex* vtx = new G4PrimaryVertex(vtx_x, vtx_y, vtx_z, 0);

  fParticleGun->SetParticlePosition(G4ThreeVector(vtx_x, vtx_y, vtx_z));
 
  // Random angle for GetFullVolume() == false case
	double angX =  359 * G4UniformRand();
	double angY =  359 * G4UniformRand();
	double angZ =  359 * G4UniformRand();

	
	for (int i = 0; i < fStdHepN; i++){ 
		if(fStdHepStatus[i] == 1) {  //se Status==1 la particella è nello stato finale dell'interazione

      G4ParticleDefinition *myParticle = G4ParticleTable::GetParticleTable()->FindParticle(fStdHepPdg[i]);
      //This is code from EDepSim to deal with nuclear PDGs
	    if (!myParticle) {
		    //maybe we have an ion; figure out if it makes any sense
		    int ionA = (fStdHepPdg[i]/10) % 1000;
		    int ionZ = (fStdHepPdg[i]/10000) % 1000;
		    int type = (fStdHepPdg[i]/100000000);
		    if (type == 10 && ionZ > 0 && ionA > ionZ) {
			    G4IonTable* ionTable = G4ParticleTable::GetParticleTable()->GetIonTable();
			    myParticle = ionTable->GetIon(ionZ,ionA);
		    }
		    else if (type == 20) {
			    // This is a pseudo-particle, so skip it
			    continue;
		    }
	    }

      // If GetFullVolume() == false rotate the impulse vector
      TMatrixD impulse(1,3);
      if (OptMenReadParameters::Get()->GetFullVolume() == false) {
			  impulse = rotateVector(fStdHepP4[i][0], fStdHepP4[i][1], fStdHepP4[i][2], angX, angY, angZ);
        std::cout << "Rotatig impulse.." << std::endl;
      } else {
			  impulse = rotateVector(fStdHepP4[i][0], fStdHepP4[i][1], fStdHepP4[i][2], 0, 0, 0);
      }

      countTrack++;
			  
      G4PrimaryParticle* particle = new G4PrimaryParticle(myParticle, impulse[0][0]*GeV, impulse[0][1]*GeV, impulse[0][2]*GeV, fStdHepP4[i][3]*GeV); //l'impulso è in GeV
		  
      std::cout<<"Particella nello stato finale "<< fStdHepPdg[i] <<std::endl;
	    std::cout << "Particella generata: "<< fStdHepPdg[i] << std::endl;
      std::cout << "Impulso originale: " << fStdHepP4[i][0] << " " <<fStdHepP4[i][1] << " " << fStdHepP4[i][2] << std::endl;
      std::cout << "Impulso finale: " << impulse[0][0] << " " << impulse[0][1] << " " << impulse[0][2] << std::endl;
      std::cout << "Energia: " << fStdHepP4[i][3] << std::endl;

			vtx->SetPrimary( particle );
		}
	}

	event->AddPrimaryVertex( vtx );

  // Used to get the correst entry from the file. Not increasing if GetStartingOnly() == true
	if (OptMenReadParameters::Get()->GetStartingOnly() == false) eventIndex++;
  // Used to count the simulated event
  eventCount++;
  
	if(eventCount == nEvents) {
		fInput->Close();
		std::cout << "Last event generated: Genie file closed." << std::endl;
	}

}


void OptMenGenieGenerator::ReadTree() {

  fInput = new TFile(fFileName.Data(),"READ");
	if (!fInput->IsOpen()) {
		std::cout << "ERROR : " << fFileName << " cannot be opened! "<< std::endl;
		exit(EXIT_FAILURE);
	}

	fTree =(TTree*)fInput->Get("gRooTracker");
  

	fEvtFlags = NULL;
	fTree->SetBranchAddress("EvtFlags",       &fEvtFlags);
	fEvtCode = NULL;
	fTree->SetBranchAddress("EvtCode",        &fEvtCode);
	fTree->SetBranchAddress("EvtNum",         &fEvtNum);
	fTree->SetBranchAddress("EvtXSec",        &fEvtXSec);
	fTree->SetBranchAddress("EvtDXSec",       &fEvtDXSec);
	fTree->SetBranchAddress("EvtWght",        &fEvtWght);
	fTree->SetBranchAddress("EvtProb",        &fEvtProb);
	fTree->SetBranchAddress("EvtVtx",          fEvtVtx);
	fTree->SetBranchAddress("StdHepN",        &fStdHepN);
	fTree->SetBranchAddress("StdHepPdg",       fStdHepPdg);
	fTree->SetBranchAddress("StdHepStatus",    fStdHepStatus);
	fTree->SetBranchAddress("StdHepX4",        fStdHepX4);
	fTree->SetBranchAddress("StdHepP4",        fStdHepP4);
	fTree->SetBranchAddress("StdHepPolz",      fStdHepPolz);
	fTree->SetBranchAddress("StdHepFd",        fStdHepFd);
	fTree->SetBranchAddress("StdHepLd",        fStdHepLd);
	fTree->SetBranchAddress("StdHepFm",        fStdHepFm);
	fTree->SetBranchAddress("StdHepLm",        fStdHepLm);
	fTree->SetBranchAddress("G2NeutEvtCode",   &fG2NeutEvtCode);

	fIsFirstTime = false ;

}

TMatrixD OptMenGenieGenerator::rotateVector(double x, double y, double z, double angX, double angY, double angZ) {
    angX = angX / 180 * M_PI;
    angY = angY / 180 * M_PI;
    angZ = angZ / 180 * M_PI;

    TArrayD xArray(9);
    xArray[0] = 1;
    xArray[1] = 0;
    xArray[2] = 0;
    xArray[3] = 0;
    xArray[4] = cos(angX);
    xArray[5] = -sin(angX);
    xArray[6] = 0;
    xArray[7] = sin(angX);
    xArray[8] = cos(angX);

    TMatrixD xMat(3, 3);
    xMat.SetMatrixArray(xArray.GetArray());
    
    TArrayD yArray(9);
    yArray[0] = cos(angY);
    yArray[1] = 0;
    yArray[2] = sin(angY);
    yArray[3] = 0;
    yArray[4] = 1;
    yArray[5] = 0;
    yArray[6] = -sin(angY);
    yArray[7] = 0;
    yArray[8] = cos(angY);

    TMatrixD yMat(3, 3);
    yMat.SetMatrixArray(yArray.GetArray());

    TArrayD zArray(9);
    zArray[0] = cos(angZ);
    zArray[1] = -sin(angZ);
    zArray[2] = 0;
    zArray[3] = sin(angZ);
    zArray[4] = cos(angZ);
    zArray[5] = 0;
    zArray[6] = 0;
    zArray[7] = 0;
    zArray[8] = 1;

    TMatrixD zMat(3, 3);
    zMat.SetMatrixArray(zArray.GetArray());
  
    TMatrixD rotMatrix = xMat * yMat * zMat;

    TArrayD startingDirectionVec(3);
    startingDirectionVec[0] = x;
    startingDirectionVec[1] = y;
    startingDirectionVec[2] = z;

    TMatrixD startingDirection(1,3);
    startingDirection.SetMatrixArray(startingDirectionVec.GetArray());

    TMatrixD rotatedDirection(1,3);
    
    rotatedDirection = startingDirection * rotMatrix;
    // rotatedDirection.Print();

    return rotatedDirection;
}
