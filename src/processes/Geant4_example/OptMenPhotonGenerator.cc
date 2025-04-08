// --------------------------------------------------------------------------//
#include "G4Event.hh"
#include "G4ParticleGun.hh"
#include "G4IonTable.hh"
#include "G4ParticleTable.hh"
#include "G4ParticleDefinition.hh"
#include "OptMenPhotonGenerator.hh"
#include "G4SystemOfUnits.hh"
#include <cmath>
#include <vector>
#include <list>
#include "OptMenReadParameters.hh"
#include "TTreeReader.h"
#include "TH1D.h"
#include "TTreeReaderValue.h"
#include "TSystem.h"
#include "TGeoEltu.h"
#include "G4Navigator.hh"
#include "G4MaterialPropertiesTable.hh"
#include "TTreeReaderArray.h"
#include "G4TransportationManager.hh"
#include "G4VPhysicalVolume.hh"
#include "G4Poisson.hh"

#include <EdepReader/EdepReader.hpp>
#include <ufw/context.hpp>

G4Mutex	lmutex = G4MUTEX_INITIALIZER;

//---------------------------------------------------------------------------//

OptMenPhotonGenerator::OptMenPhotonGenerator() : OptMenVGenerator("OptMenPhotonGenerator") {

    // fParticleGun  = new G4ParticleGun;
    fParticleTable = G4ParticleTable::GetParticleTable();
    fParticleGun.SetParticleDefinition(fParticleTable->FindParticle("geantino"));
    found = false;

    fFileName = OptMenReadParameters::Get()->GetInputFile();
    startingEntry = OptMenReadParameters::Get()->GetStartingEntry();

    // if(!fParticleGun) {
    //     std::cout << "Could not allocate G4ParticleGun! Out of memory?"<< std::endl;
    //     exit(EXIT_FAILURE);
    // }

    std::cout << "OptMenPhotonGenerator constructed!" << std::endl;
}
//---------------------------------------------------------------------------//

OptMenPhotonGenerator::~OptMenPhotonGenerator()
{
    //G4AutoLock l(&lmutex);
    // delete fParticleGun;
    // delete fParticleTable;
    // delete fInput;
    // delete fEDepSimEvents;
    // delete fastComponentHisto;
    // delete slowComponentHisto;
}

int OptMenPhotonGenerator::eventIndex = 0;
int OptMenPhotonGenerator::currentHit = 0;
int OptMenPhotonGenerator::subEventNumber = 0;
//---------------------------------------------------------------------------//

void OptMenPhotonGenerator::ReadEDepSimEvent(){    
//     fInput = new TFile(fFileName.c_str(), "READ");
//     if(!fInput->IsOpen()){
// 		std::cout << "ERROR : " << fFileName << " cannot be opened! "<< std::endl;
// 		exit(EXIT_FAILURE);
// 	}

// 	std::cout << "Opening the EVENT source file: " << fFileName << std::endl;
	
// 	fEDepSimEvents = (TTree*) fInput->Get("EDepSimEvents");
// 	if(!fEDepSimEvents){
// 		std::cout << "ERROR : EDepSimEvents tree not found!"<< std::endl;
// 		exit(EXIT_FAILURE);
// 	}

//     gSystem->Load("libGeom");
//     TGeoManager::Import(fFileName.c_str());
    
//     //new GRAIN geometry 
//   gGeoManager->cd("volWorld_PV/rockBox_lv_PV_0/volDetEnclosure_PV_0/volSAND_PV_0/MagIntVol_volume_PV_0/sand_inner_volume_PV_0/GRAIN_lv_PV_0/GRAIN_LAr_lv_PV_0");    
// //gGeoManager->cd("volWorld_PV_1/rockBox_lv_PV_0/volDetEnclosure_PV_0/volSAND_PV_0/MagIntVol_volume_PV_0/sand_inner_volume_PV_0/GRAIN_lv_PV_0/GRAIN_Ext_vessel_outer_layer_lv_PV_0/GRAIN_Honeycomb_layer_lv_PV_0/GRAIN_Ext_vessel_inner_layer_lv_PV_0/GRAIN_gap_between_vessels_lv_PV_0/GRAIN_inner_vessel_lv_PV_0/GRIAN_LAr_lv_PV_0");
//     //old GRAIN geometry 
//     //gGeoManager->cd("volWorld_PV_1/rockBox_lv_PV_0/volDetEnclosure_PV_0/volKLOE_PV_0/MagIntVol_volume_PV_0/sand_inner_volume_PV_0/GRAIN_lv_PV_0/GRAIN_Ext_vessel_outer_layer_lv_PV_0/GRAIN_Honeycomb_layer_lv_PV_0/GRAIN_Ext_vessel_inner_layer_lv_PV_0/GRAIN_gap_between_vessels_lv_PV_0/GRAIN_inner_vessel_lv_PV_0/GRAIN_LAr_lv_PV_0");
//     //old geometry
//     //gGeoManager->cd("volWorld_PV_1/rockBox_lv_PV_0/volDetEnclosure_PV_0/volKLOE_PV_0/MagIntVol_volume_PV_0/volSTTLAR_PV_0/Gr_ext_lv_PV_0/Empty_tgt_lv_PV_0/Al_int_lv_PV_0/Lar_bulk_lv_PV_0");
    
//     gGeoManager->LocalToMaster(local, master);
//     // std::cout << master[0] << " " << master[1] << " " << master[2] <<  std::endl;

    
//     fEvent = new TG4Event();
//     fEDepSimEvents->SetBranchAddress("Event",&fEvent);
}

//-------------------------------------------------------------------------//

void OptMenPhotonGenerator::GetEntry(){
    auto& tree = ufw::context::instance<sand::EdepReader>();


    // fEDepSimEvents->GetEntry(eventIndex + startingEntry);
    
    fNHits = 0;
    fTotEnDep = 0;
    fTotSecondaryEnDep = 0;
    subEventNumber++;

    found = true;

    std::vector<int> vec = OptMenReadParameters::Get()->GetIdList();
    if (OptMenReadParameters::Get()->GetIDlistFile() == true) {
        if (std::find(vec.begin(), vec.end(), eventIndex + startingEntry) != vec.end()) {
            std::cout << "Requested event " << eventIndex + startingEntry << ". It was found in the event list and it will be processed." << std::endl;
            found = true;
        } else {
            std::cout << "Requested event " << eventIndex + startingEntry << ". It was NOT found in the event list and it will NOT be processed." << std::endl;
            found = false;
        }
    }

    for (const auto& trj : tree) {
        std::cout << eventIndex + startingEntry << " " << subEventNumber << std::endl;

        int id = trj.GetId();
        int PDG = trj.GetPDGCode();
        double energy = trj.GetInitialMomentum().E();

        //if track is a primary contributor && not already recorded
        if( std::find(fPrimaryID.begin(),fPrimaryID.end(),id) != fPrimaryID.end()
                && fPrimaryPDG.find(id) == fPrimaryPDG.end() ){

            fPrimaryPDG.insert(std::make_pair(id, PDG));
            fInitialEnergy.insert(std::make_pair(id, energy));
            std::cout << "Track: " << id << " PDG: " << PDG << " Energy: " << energy << std::endl;
        }

        for (const auto& hit : trj.GetHitMap().at(component::GRAIN)) {
            fDetName = component_to_string[component::GRAIN];
            
            fPrimaryID.push_back(hit.GetPrimaryId());
            fContribID[hit.GetId()] = std::vector<int> (1, hit.GetContrib());
            fStart.push_back(hit.GetStart());
            fStop.push_back(hit.GetStop());
            fStepLength.push_back(hit.GetTrackLength());
            fEnDep.push_back(hit.GetEnergyDeposit());
            fSecondaryEnDep.push_back(hit.GetSecondaryDeposit());
            fTotEnDep += hit.GetEnergyDeposit();
            fTotSecondaryEnDep += hit.GetSecondaryDeposit();
            fNHits += 1;
        }
        std::cout << fNHits << std::endl;
    }
}

//-------------------------------------------------------------------------//

void OptMenPhotonGenerator::ApplyTranslation(){

    fStartTranslated.resize(fNHits);
    fStopTranslated.resize(fNHits);

    for (int i = 0; i < fNHits; i++) {
        fStartTranslated.at(i).setX(fStart.at(i).X() - master[0]);
        fStartTranslated.at(i).setY(fStart.at(i).Y() - master[1]);
        fStartTranslated.at(i).setZ(fStart.at(i).Z() - master[2]);

        fStopTranslated.at(i).setX(fStop.at(i).X() - master[0]);
        fStopTranslated.at(i).setY(fStop.at(i).Y() - master[1]);
        fStopTranslated.at(i).setZ(fStop.at(i).Z() - master[2]);
    }
}

//-------------------------------------------------------------------------//

bool OptMenPhotonGenerator::ApplyVolumeCut(G4ThreeVector pos){

      	//reference: Geant4 User's Guide for Application Developers, Detector Definition and Response, 4.1.8.2
        //se usi il navigatore del tracking durante il tracking, causi problemi... qui però non dovrebbe darne
        //in caso ne dia, bisogna creare un nuovo navigatore a cui associare il volume del mondo
	G4Navigator* tracking_navigator = G4TransportationManager::GetTransportationManager()->GetNavigatorForTracking();
	G4VPhysicalVolume* myVolume  = tracking_navigator->LocateGlobalPointAndSetup(pos);

	std::string matname =  myVolume->GetLogicalVolume()->GetMaterial()->GetName();
	//std::cout << myVolume->GetName() << " " << myVolume->GetLogicalVolume()->GetName() << " " << matname << std::endl;
 	
	//true if it must be skipped becausa it's not emitted in LAr
	if(matname.find("G4_lAr") == std::string::npos) return true;
	else return false;         
}

//-------------------------------------------------------------------------//

std::pair<G4ThreeVector,G4ThreeVector> OptMenPhotonGenerator::GenerateRandomMomentumPolarization(){

	G4double cost = 1 - 2*G4UniformRand();
	G4double sint = sqrt( 1 - cost*cost );

	G4double phi  = 2*M_PI*G4UniformRand();
	G4double cosp = cos( phi );
	G4double sinp = sin( phi );

	G4double px = sint*cosp;
	G4double py = sint*sinp;
	G4double pz = cost;

	G4ThreeVector myPhotonMomentum ( px, py, pz );
	
	G4double sx = cost*cosp;
	G4double sy = cost*sinp;
	G4double sz = -sint;
	
	G4ThreeVector myPhotonPolarization ( sx, sy, sz );
	G4ThreeVector myPerpendicular = myPhotonMomentum.cross( myPhotonPolarization );

	phi  = 2*M_PI*G4UniformRand() ;
	cosp = cos( phi ) ;
	sinp = sin( phi ) ;

	myPhotonPolarization = cosp * myPhotonPolarization + sinp * myPhotonMomentum ;
	myPhotonPolarization = myPhotonPolarization.unit();
	
	return std::make_pair(myPhotonMomentum, myPhotonPolarization);
}

//--------------------------------------------------------------------------//

void OptMenPhotonGenerator::GeneratePrimaries(G4Event *event) {
    //G4AutoLock l(&lmutex);

    event->SetEventID(eventIndex + startingEntry);

    // Read EdepSim file
    ReadEDepSimEvent();

    // Read EdepSim energy deposits
    GetEntry();

    tmpEnDep = 0;
    tmpSecondaryEnDep = 0;

    //apply shift 
    ApplyTranslation();

    // Get lAr info
    getMaterialProperties();

    if (fNHits > 0 && found) {
        std::cout << "Processing emitted photons from hit: " << currentHit << std::endl;
        for (int i = currentHit; i < fNHits; i++) {
            G4int myPDG = fPrimaryPDG[fPrimaryID[i]];
            G4ParticleDefinition *myParticle = G4ParticleTable::GetParticleTable()->FindParticle(myPDG);	

	        //This is code from EDepSim to deal with nuclear PDGs
	        if (!myParticle) {
		        //maybe we have an ion; figure out if it makes any sense
		        int ionA = (myPDG/10) % 1000;
		        int ionZ = (myPDG/10000) % 1000;
		        int type = (myPDG/100000000);
		        if (type == 10 && ionZ > 0 && ionA > ionZ) {
		        	G4IonTable* ionTable = G4ParticleTable::GetParticleTable()->GetIonTable();
		        	myParticle = ionTable->GetIon(ionZ,ionA);
	        	}
	        	else if (type == 20) {
		        	// This is a pseudo-particle, so skip it
	        		continue;
	        	}
	        }

	        G4int myZ = myParticle->GetAtomicNumber();
            G4double myMass = myParticle->GetPDGMass();

            //initial kinetic energy of track
            G4double myVertexKinEne = fInitialEnergy[fPrimaryID[i]] - myMass;                   

            //COMPUTATION FOR NUMBER of PHOTONS
            // The fraction of the energy deposit that ends up producing photons is already computed by EDepSim
            // using the Doke-Birks NEST model and store in "SecondaryDeposit".
            // This ALREADY takes into account excitons and the fraction of recombinating ions 
            
            // photons (excitons + recombinating ions)
            G4double myphotons = fSecondaryEnDep[i]*fScintillationYield;
            
	        int myNumPhotons = 0;
            if(myphotons < 20)     myNumPhotons = int(G4Poisson(myphotons) + 0.5);
            else                   myNumPhotons = int(G4RandGauss::shoot(myphotons, sqrt(myphotons))+0.5);
            if(myNumPhotons < 0)   myNumPhotons = 0 ;
     
            //TODO: check better sources
            // Xe-DOPING --> Xe-doping increases the overall LY to 1.20 pure LAr
            // but this is slow component only (fast is suppressed?)
            // LY = 0.25(fast) + 0.75(slow) + addition = 1.20
            int myXeAddition = 0;
            if( OptMenReadParameters::Get()->GetXeDoping() ){
		
		        double XeAddition = 0.2*myphotons;
            	if(XeAddition < 20)     myXeAddition = int(G4Poisson(XeAddition) + 0.5);
            	else                    myXeAddition = int(G4RandGauss::shoot(XeAddition, sqrt(XeAddition))+0.5);
            	if(myXeAddition < 0)    myXeAddition = 0 ;
      	    }
		
            // slow/fast components ratio
            G4double mySingletTripletRatio = GetSingletTripletRatio(myZ, fEnDep[i], myVertexKinEne);

            //G4cout << "Step from ID " << fPrimaryID.at(i) << " " << i << " / " << fNHits << G4endl;
            //G4cout << "Pos:" << fStartTranslated.at(i) << " -> " << fStopTranslated.at(i) << G4endl;;
            //std::cout << "EnDep: " << fEnDep[i] << " SecondaryEnDep: " << fSecondaryEnDep[i] << " MeV" << std::endl;
            //std::cout << "Mean number of photons: " << myphotons << std::endl;
            //std::cout << "Number of EMITTED photons: " << myNumPhotons << std::endl;
            //std::cout << "Effective LY: " << myNumPhotons/fSecondaryEnDep[i] << std::endl;
            //std::cout << "Fast/slow ratio: " << mySingletTripletRatio << std::endl;

            G4ParticleDefinition *particle = G4ParticleTable::GetParticleTable()->FindParticle("opticalphoton"); 
            fParticleGun.SetParticleDefinition(particle);

            for(int j=0; j< myNumPhotons; j++){

                // Momentum & Polarization
 	            std::pair<G4ThreeVector,G4ThreeVector>  myMomentumPolarization = GenerateRandomMomentumPolarization();
                fParticleGun.SetParticleMomentumDirection(myMomentumPolarization.first);
		        fParticleGun.SetParticlePolarization(myMomentumPolarization.second);

                G4double random = G4UniformRand(); //random between 0 and 1

                // Position
                G4ThreeVector myPhotonPosition = fStartTranslated.at(i) + random*( fStopTranslated.at(i) - fStartTranslated.at(i) );
		        if(ApplyVolumeCut(myPhotonPosition)) continue; // skip if not in LAr
                fParticleGun.SetParticlePosition(myPhotonPosition);					

	            // Time & Energy
                // photonTime = Global Time ( + Recombination Time ) + Singlet/Triplet Time
                G4double myPhotonTime = fStart.at(i).T() + random*(fStop.at(i).T() - fStart.at(i).T());
                G4double mySampledEnergy = 0;

                // applies scintillation constant according to singlet/triplet ratio
                // determines energy spectrum accordingly
                if( G4UniformRand() < mySingletTripletRatio ){
 
                //FIXME: uncertain (reported only by protoDUNE) -> investigate
                //Xe-DOPING: Xe-doping below 1000ppm does not shift the fast component
                //but also reduces it to the absorption. From protoDUNE plots, it seems a 50% reduction
                /*if( OptMenReadParameters::Get()->GetXeDoping() ) {
                    if( G4UniformRand() < 0.5 ) continue; //skip, don't emit
                }*/

                    myPhotonTime -= fTauFast * log( G4UniformRand() );
                    mySampledEnergy = fastComponentHisto->GetRandom();				
                }
                else{
                    myPhotonTime -= fTauSlow * log( G4UniformRand() );
                    mySampledEnergy = slowComponentHisto->GetRandom();				
                }

                fParticleGun.SetParticleEnergy(mySampledEnergy);
                fParticleGun.SetParticleTime(myPhotonTime);		

                //SHOOT PHOTON!!
                fParticleGun.GeneratePrimaryVertex(event);
            }

            // TODO: check better sources
            // Xe-DOPING --> Xe-doping increases the overall LY to 1.20 pure LAr
            // emit here the additional photons which are all slow component
            for(int j = 0; j < myXeAddition; j++){
                
                // Momentum & Polarization
 	            std::pair<G4ThreeVector,G4ThreeVector>  myMomentumPolarization = GenerateRandomMomentumPolarization();
                fParticleGun.SetParticleMomentumDirection(myMomentumPolarization.first);
		        fParticleGun.SetParticlePolarization(myMomentumPolarization.second);

                G4double random = G4UniformRand(); //random between 0 and 1

                // Position
                G4ThreeVector myPhotonPosition = fStartTranslated.at(i) + random*( fStopTranslated.at(i) - fStartTranslated.at(i) );
		        if(ApplyVolumeCut(myPhotonPosition)) continue; // skip if not in LAr
                fParticleGun.SetParticlePosition(myPhotonPosition);					

	            // Time & Energy
                // (slow component only)
                G4double myPhotonTime = fStart.at(i).T() + random*(fStop.at(i).T() - fStart.at(i).T()) - fTauSlow * log( G4UniformRand() );
                G4double mySampledEnergy = slowComponentHisto->GetRandom();				

                fParticleGun.SetParticleEnergy(mySampledEnergy);
                fParticleGun.SetParticleTime(myPhotonTime);		

                //SHOOT PHOTON!!
                fParticleGun.GeneratePrimaryVertex(event);
	        }
	
	        tmpEnDep += fEnDep[i];
            tmpSecondaryEnDep += fSecondaryEnDep[i];
            currentHit++;

            if (currentHit == fNHits) {
                eventIndex++;
                currentHit = 0;
                std::cout << "Reached end of lAr hits. Resetting currentHit to " << currentHit << std::endl;
                break;
            }

            if (tmpEnDep > OptMenReadParameters::Get()->GetEnergySplitThreshold()) {
                std::cout << "Reached " <<  tmpEnDep << " MeV. CurrentHit: " << currentHit << std::endl;
                break;
            }			
        }
    } else {
        if (fNHits > 0) {
            for (int i = currentHit; i < fNHits; i++) {
                tmpEnDep += fEnDep[i];
                tmpSecondaryEnDep += fSecondaryEnDep[i];
                currentHit++;

                G4ParticleDefinition* particleDefinition = G4ParticleTable::GetParticleTable()->FindParticle("geantino");
                fParticleGun.SetParticleDefinition(particleDefinition);
                fParticleGun.SetParticleMomentumDirection(G4ThreeVector(0.,0.,1.));
                fParticleGun.SetParticleEnergy(1*GeV);
                fParticleGun.SetParticlePosition(G4ThreeVector(0., 0., -500));
                fParticleGun.GeneratePrimaryVertex(event);
                
                if (currentHit == fNHits) {
                    eventIndex++;
                    currentHit = 0;
                    std::cout << "Reached end of not processed event. Resetting currentHit to " << currentHit << std::endl;
                    break;
                }

                if (tmpEnDep > OptMenReadParameters::Get()->GetEnergySplitThreshold()) {
                    std::cout << "Reached " <<  tmpEnDep << " MeV. CurrentHit: " << currentHit << std::endl;
                    break;
                }		
            }
        } else {
            eventIndex++;
            G4ParticleDefinition* particleDefinition = G4ParticleTable::GetParticleTable()->FindParticle("geantino");
            fParticleGun.SetParticleDefinition(particleDefinition);
            fParticleGun.SetParticleMomentumDirection(G4ThreeVector(0.,0.,1.));
            fParticleGun.SetParticleEnergy(1*GeV);
            fParticleGun.SetParticlePosition(G4ThreeVector(0., 0., -500));
            fParticleGun.GeneratePrimaryVertex(event);
        }
        
    }

    clear();
    fInput->Close();
    std::cout << "---------> Fine generazione fotoni <---------------" << std::endl;
}

void OptMenPhotonGenerator::getMaterialProperties() {

    G4NistManager* manager = G4NistManager::Instance();
    G4Material* mat = manager->FindOrBuildMaterial("G4_lAr");
    G4MaterialPropertiesTable* matTable = mat->GetMaterialPropertiesTable();
    G4MaterialPropertyVector* property;

    property = matTable->GetProperty("FASTCOMPONENT");
    fastComponentHisto = new TH1D("FASTCOMPONENT", "FASTCOMPONENT", property->GetVectorLength(), property->GetMinLowEdgeEnergy(), property->GetMaxEnergy());
    for (int i = 0; i < property->GetVectorLength(); i++) 
        fastComponentHisto->SetBinContent(i + 1, property->Value(property->Energy(i)));

    property = matTable->GetProperty("SLOWCOMPONENT");
    slowComponentHisto = new TH1D("SLOWCOMPONENT", "SLOWCOMPONENT", property->GetVectorLength(), property->GetMinLowEdgeEnergy(), property->GetMaxEnergy());
    for (int i = 0; i < property->GetVectorLength(); i++) 
        slowComponentHisto->SetBinContent(i + 1, property->Value(property->Energy(i)));

    fTauFast = matTable->GetConstProperty("FASTTIMECONSTANT");
    fTauSlow = matTable->GetConstProperty("SLOWTIMECONSTANT");
    fScintillationYield = matTable->GetConstProperty("SCINTILLATIONYIELD");
}

void OptMenPhotonGenerator::clear() {

    fContribID.clear();
    fPrimaryPDG.clear();
    fInitialEnergy.clear();
    std::vector<int>().swap(fPrimaryID);
    std::vector<TLorentzVector>().swap(fStart);
    std::vector<TLorentzVector>().swap(fStop);
    std::vector<G4ThreeVector>().swap(fStartTranslated);
    std::vector<G4ThreeVector>().swap(fStopTranslated);
    std::vector<double>().swap(fStepLength);
    std::vector<double>().swap(fEnDep);
    std::vector<double>().swap(fSecondaryEnDep);
    fNHits = 0;
}


/////////////////////////////////////////////////////////////////////////////////
//funzioni per determinare SINGLET to TRIPLET ratio necessario per la scelta
//della costante di scintillazione (COPIED from DuLight3.cc)
////////////////////////////////////////////////////////////////////////////////

G4double OptMenPhotonGenerator::getERf90 (double ene) {
    double f90p0 = 0.249488;
    double f90p1 = 0.146597;
    double f90p2 = -2.89121;
    return f90p0 * (1 + exp(( -f90p1*(ene - f90p2))));
}


G4double OptMenPhotonGenerator::GetSingletTripletRatio(double myZ, double myDepEne, double VertexKinEne){

    double mySingletTripletRatio=0;

    //-- Electron Recoils
    if( myZ != 18 && myZ != 2 ){
        mySingletTripletRatio = getERf90( myDepEne /keV ) ;
        //ci va myNumQuanta*fMeanQuantaEnergy: ma myNumQuanta = fQuenchingFactor*myDepEne/fMeanQuantaEnergy
        //quindi solo fQuenchingFactor*myDepEne con fQuenchingFactor = 1
    }
    // -- Alphas
    else if( myZ == 2 ){
        mySingletTripletRatio = (-0.065492+1.9996*exp(-myDepEne/MeV))/(1+0.082154/pow(myDepEne/MeV,2.)) + 2.1811; 
    }
    //-- Nuclear Recoils
    else{
        double myVisEne = VertexKinEne /keV; 
        //sarebbe InitialKinEne/fQuenchingFactor, ma InitialKinEne = VertexKinEne * fQuenchingFactor....
        if (myVisEne>180 ) myVisEne = 180 ;

        double ratio_p0                        =     0.513575   ;///-   0.00935121  
        double ratio_p1                        =   0.00664834   ;///-   0.000618988 
        double ratio_p2                        = -7.26861e-05   ;///-   1.22681e-05 
        double ratio_p3                        =  3.69379e-07   ;///-   9.10873e-08 
        double ratio_p4                        = -7.04932e-10   ;///-   2.24589e-10 
        mySingletTripletRatio = ratio_p0 - 0.045 +  ratio_p1*myVisEne + ratio_p2*pow(myVisEne,2) +  ratio_p3*pow(myVisEne,3 )
            + ratio_p4*pow(myVisEne , 4) ;
    }

    return mySingletTripletRatio;

}
