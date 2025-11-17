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
/// \file PrimaryGeneratorAction.cc
/// \brief Implementation of the PrimaryGeneratorAction class

#include "PrimaryGeneratorAction.hh"

#include "G4Event.hh"
#include "G4ParticleGun.hh"
#include "G4IonTable.hh"
#include "G4ParticleTable.hh"
#include "G4ParticleDefinition.hh"
#include "PrimaryGeneratorAction.hh"
#include "G4SystemOfUnits.hh"
#include <cmath>

#include "G4Navigator.hh"
#include "G4TransportationManager.hh"
#include "G4VPhysicalVolume.hh"
#include "G4Poisson.hh"

#include <ufw/context.hpp>
#include <optical_simulation.hpp>
#include <geoinfo/grain_info.hpp>

namespace sand::grain {
PrimaryGeneratorAction::PrimaryGeneratorAction(optical_simulation* optmen_edepsim) : m_optmen_edepsim(optmen_edepsim) {
	fParticleTable = G4ParticleTable::GetParticleTable();
	fParticleGun.SetParticleDefinition(fParticleTable->FindParticle("geantino"));
}

PrimaryGeneratorAction::~PrimaryGeneratorAction() {}

void PrimaryGeneratorAction::ApplyTranslation(){
    const auto& geom = m_optmen_edepsim->instance<geoinfo>();
    geom.grain().transform().GetTranslation(m_centre);
}

void PrimaryGeneratorAction::nextIteration() {
    const auto& tree = m_optmen_edepsim->instance<sand::edep_reader>();
    
    if(m_optmen_edepsim->getStartRun()) {
        m_tree_it = tree.begin();
        m_optmen_edepsim->setStartRun(false);
    }
    
    if(m_tree_it == tree.end()) {
        UFW_FATAL("Reached end of tree");
    }

    if (m_tree_it->GetHitMap().find(component::GRAIN) == m_tree_it->GetHitMap().end()) {
        m_tree_it++;
        m_optmen_edepsim->setNewIteration(true);
        nextIteration();
        return;
    } else {
        const auto& hit_map = m_tree_it->GetHitMap();
        const auto& hit_vect = hit_map.at(component::GRAIN);
        if(m_optmen_edepsim->getNewIteration()) {
            m_hits_it = hit_vect.begin();
            m_optmen_edepsim->setNewIteration(false);
            return;
        } else {
            m_hits_it++;
            if (m_hits_it == hit_vect.end()) {
                m_tree_it++;
                m_optmen_edepsim->setNewIteration(true);
                nextIteration();
                return;
            }
            return;
        }
    }
}

bool isInArgon(const G4ThreeVector& myPhotonPosition) 
{
    G4Navigator* tracking_navigator = G4TransportationManager::GetTransportationManager()->GetNavigatorForTracking();
    G4VPhysicalVolume* myVolume  = tracking_navigator->LocateGlobalPointAndSetup(myPhotonPosition);

    std::string matname =  myVolume->GetLogicalVolume()->GetMaterial()->GetName();
    
    //true if it must be skipped becausa it's not emitted in LAr
    return (matname.find("G4_lAr") != std::string::npos);
}

std::pair<G4ThreeVector,G4ThreeVector> PrimaryGeneratorAction::GenerateRandomMomentumPolarization(){

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

void PrimaryGeneratorAction::GeneratePrimaries(G4Event *event) {

    event->SetEventID(ufw::context::current()->id());

    // Get lAr info
    m_optmen_edepsim->properties();
    
    nextIteration();
    
    //apply shift 
    ApplyTranslation();

    G4int myPDG = m_hits_it->GetPrimaryId();
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
            if (!myParticle) {
                UFW_WARN("Particle '{}' not found. Tried getIon({}, {})", myPDG, ionZ, ionA);
                return;
            }
        }
        else if (type == 20) {
            // This is a pseudo-particle, so skip it
            return;
        } else {
            UFW_WARN("Particle '{}' not found. type {} not supported", myPDG, type);
            return;

        }
    }

    G4int myZ = myParticle->GetAtomicNumber();
    G4double myMass = myParticle->GetPDGMass();

    //initial kinetic energy of track
    G4double myVertexKinEne = m_tree_it->GetInitialMomentum().E();

    // COMPUTATION FOR NUMBER of PHOTONS
    // The fraction of the energy deposit that ends up producing photons is already computed by EDepSim
    // using the Doke-Birks NEST model and store in "SecondaryDeposit".
    // This ALREADY takes into account excitons and the fraction of recombinating ions 
    
    // photons (excitons + recombinating ions)
    G4double myphotons = m_hits_it->GetSecondaryDeposit() * m_optmen_edepsim->properties().m_scintillation_yield;
    
    int myNumPhotons = 0;
    if(myphotons < 20)     myNumPhotons = int(G4Poisson(myphotons) + 0.5);
    else                   myNumPhotons = int(G4RandGauss::shoot(myphotons, sqrt(myphotons))+0.5);
    if(myNumPhotons < 0)   myNumPhotons = 0 ;

    UFW_DEBUG("Shooting {} photons", myNumPhotons);

    //TODO: check better sources
    // Xe-DOPING --> Xe-doping increases the overall LY to 1.20 pure LAr
    // but this is slow component only (fast is suppressed?)
    // LY = 0.25(fast) + 0.75(slow) + addition = 1.20
    int myXeAddition = 0;
    if (m_optmen_edepsim->opticsType() == optical_simulation::OpticsType::LENS_DOPED){

        double XeAddition = 0.2*myphotons;
        if(XeAddition < 20)     myXeAddition = int(G4Poisson(XeAddition) + 0.5);
        else                    myXeAddition = int(G4RandGauss::shoot(XeAddition, sqrt(XeAddition))+0.5);
        if(myXeAddition < 0)    myXeAddition = 0 ;
    }

    // slow/fast components ratio
    G4double mySingletTripletRatio = GetSingletTripletRatio(myZ, m_hits_it->GetEnergyDeposit(), myVertexKinEne);

    G4ParticleDefinition *particle = G4ParticleTable::GetParticleTable()->FindParticle("opticalphoton"); 
    fParticleGun.SetParticleDefinition(particle);

    for(int j = 0; j< myNumPhotons; j++){

        // Momentum & Polarization
        std::pair<G4ThreeVector,G4ThreeVector>  myMomentumPolarization = GenerateRandomMomentumPolarization();
        fParticleGun.SetParticleMomentumDirection(myMomentumPolarization.first);
        fParticleGun.SetParticlePolarization(myMomentumPolarization.second);

        G4double random = G4UniformRand(); //random between 0 and 1

        // Position
        G4ThreeVector translated_start(m_hits_it->GetStart().X() - m_centre.x(), m_hits_it->GetStart().Y() - m_centre.y(), m_hits_it->GetStart().Z() - m_centre.z());
        G4ThreeVector translated_stop(m_hits_it->GetStop().X() - m_centre.x(), m_hits_it->GetStop().Y() - m_centre.y(), m_hits_it->GetStop().Z() - m_centre.z());
        G4ThreeVector myPhotonPosition = translated_start + random * (translated_stop - translated_start);

        if(!isInArgon(myPhotonPosition)) continue; // skip if not in LAr
        fParticleGun.SetParticlePosition(myPhotonPosition);					

        // Time & Energy
        // photonTime = Global Time ( + Recombination Time ) + Singlet/Triplet Time
        G4double myPhotonTime = m_hits_it->GetStart().T() + random*(m_hits_it->GetStop().T() - m_hits_it->GetStart().T());
        G4double mySampledEnergy = 0;

        // applies scintillation constant according to singlet/triplet ratio
        // determines energy spectrum accordingly
        if( G4UniformRand() < mySingletTripletRatio ){

        //FIXME: uncertain (reported only by protoDUNE) -> investigate
        //Xe-DOPING: Xe-doping below 1000ppm does not shift the fast component
        //but also reduces it to the absorption. From protoDUNE plots, it seems a 50% reduction
        /*if( ReadParameters::Get()->GetXeDoping() ) {
            if( G4UniformRand() < 0.5 ) continue; //skip, don't emit
        }*/

            myPhotonTime -= m_optmen_edepsim->properties().m_tau_fast * log(G4UniformRand());
            mySampledEnergy = m_optmen_edepsim->properties().m_fast_component_distribution->GetRandom();
        }
        else{
            myPhotonTime -= m_optmen_edepsim->properties().m_tau_slow * log(G4UniformRand());
            mySampledEnergy = m_optmen_edepsim->properties().m_slow_component_distribution->GetRandom();
        }

        fParticleGun.SetParticleEnergy(mySampledEnergy);
        fParticleGun.SetParticleTime(myPhotonTime);		

        //SHOOT PHOTON!!
        fParticleGun.GeneratePrimaryVertex(event);
        m_optmen_edepsim->track_times.push_back(myPhotonTime);
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
        G4ThreeVector translated_start(m_hits_it->GetStart().X() - m_centre.x(), m_hits_it->GetStart().Y() - m_centre.y(), m_hits_it->GetStart().Z() - m_centre.z());
        G4ThreeVector translated_stop(m_hits_it->GetStop().X() - m_centre.x(), m_hits_it->GetStop().Y() - m_centre.y(), m_hits_it->GetStop().Z() - m_centre.z());
        G4ThreeVector myPhotonPosition = translated_start + random * (translated_stop - translated_start);
        if(!isInArgon(myPhotonPosition)) continue; // skip if not in LAr
        fParticleGun.SetParticlePosition(myPhotonPosition);					

        // Time & Energy
        // (slow component only)
        G4double myPhotonTime = m_hits_it->GetStart().T() + random*(m_hits_it->GetStop().T() - m_hits_it->GetStart().T()) - m_optmen_edepsim->properties().m_tau_slow * log(G4UniformRand());
        G4double mySampledEnergy = m_optmen_edepsim->properties().m_slow_component_distribution->GetRandom();

        fParticleGun.SetParticleEnergy(mySampledEnergy);
        fParticleGun.SetParticleTime(myPhotonTime);		

        fParticleGun.GeneratePrimaryVertex(event);
    }
    UFW_DEBUG("Completed photons generation for hit {}", m_hits_it->GetId());
}

/////////////////////////////////////////////////////////////////////////////////
//funzioni per determinare SINGLET to TRIPLET ratio necessario per la scelta
//della costante di scintillazione (COPIED from DuLight3.cc)
////////////////////////////////////////////////////////////////////////////////

G4double PrimaryGeneratorAction::getERf90 (double ene) {
    double f90p0 = 0.249488;
    double f90p1 = 0.146597;
    double f90p2 = -2.89121;
    return f90p0 * (1 + exp(( -f90p1*(ene - f90p2))));
}


G4double PrimaryGeneratorAction::GetSingletTripletRatio(double myZ, double myDepEne, double VertexKinEne){

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
}
