#include "PhysicsList.hh"

#include "G4EmStandardPhysics.hh"

#include "G4ParticleDefinition.hh"
#include "G4ParticleTable.hh"
#include "G4ParticleTypes.hh"

#include "G4MesonConstructor.hh"
#include "G4LeptonConstructor.hh"
#include "G4BaryonConstructor.hh"
#include "G4BosonConstructor.hh"
#include "G4IonConstructor.hh"
#include "G4ShortLivedConstructor.hh"
#include "G4Geantino.hh"
#include "G4ChargedGeantino.hh"
#include "G4Gamma.hh"
#include "G4OpticalPhoton.hh"

#include "G4ProcessManager.hh"

#include "G4Cerenkov.hh"
#include "G4OpAbsorption.hh"
#include "G4OpBoundaryProcess.hh"
#include "G4OpMieHG.hh"
#include "G4OpRayleigh.hh"
#include "G4Scintillation.hh"

#include "G4EmSaturation.hh"
#include "G4LossTableManager.hh"

#include "G4ComptonScattering.hh"
#include "G4GammaConversion.hh"
#include "G4PhotoElectricEffect.hh"

#include "G4MuMultipleScattering.hh"
#include "G4eMultipleScattering.hh"
#include "G4hMultipleScattering.hh"

#include "G4eBremsstrahlung.hh"
#include "G4eIonisation.hh"
#include "G4eplusAnnihilation.hh"

#include "G4MuBremsstrahlung.hh"
#include "G4MuIonisation.hh"
#include "G4MuPairProduction.hh"

#include "G4Decay.hh"
#include "G4hIonisation.hh"

namespace sand::grain {
PhysicsList::PhysicsList() : G4VUserPhysicsList() {
  defaultCutValue = 1.0 * CLHEP::mm;  // to be defined...
  // RegisterPhysics(new G4EmStandardPhysics(0, "standard EM"));
  /*
  fScintillationProcess = 0;
  fMieHGScatteringProcess = 0;
  fAbsorptionProcess = 0;
  fRayleighScatteringProcess = 0;
  fBoundaryProcess = 0;
  */
}

PhysicsList::~PhysicsList() {}

void PhysicsList::setCuts() { SetCutsWithDefault(); }

void PhysicsList::ConstructParticle() {

  G4cout << "PARTICLE DEFINITION...." << G4endl;
  
  //  mesons
  G4MesonConstructor mConstructor;
  mConstructor.ConstructParticle();

  //  baryons
  G4BaryonConstructor bConstructor;
  bConstructor.ConstructParticle();

  //  ions
  G4IonConstructor iConstructor;
  iConstructor.ConstructParticle();

  //  leptons
  G4LeptonConstructor lConstructor;
  lConstructor.ConstructParticle();
 
  // pseudo-particles
  G4Geantino::GeantinoDefinition();
  G4ChargedGeantino::ChargedGeantinoDefinition();

  // bosons
  //G4BosonConstructor boConstructor;
  //boConstructor.ConstructParticle();
  
  // gamma
  G4Gamma::GammaDefinition();
  // optical photon
  G4OpticalPhoton::OpticalPhotonDefinition();
  
  // short lived
  G4ShortLivedConstructor  sConstructor;
  sConstructor.ConstructParticle();

}

void PhysicsList::ConstructProcess() {
  G4cout << "CONSTRUCT PROCESS...." << G4endl;
  AddTransportation();
  ConstructGeneral();
  ConstructEM();
  ConstructOp();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
void PhysicsList::ConstructGeneral() {
  // Add Decay Process
  G4Decay* theDecayProcess = new G4Decay();
  auto theParticleIterator = GetParticleIterator();
  theParticleIterator->reset();
  while ((*theParticleIterator)()) {
    G4ParticleDefinition* particle = theParticleIterator->value();
    G4ProcessManager* pmanager = particle->GetProcessManager();
    if (theDecayProcess->IsApplicable(*particle)) {
      pmanager->AddProcess(theDecayProcess);
      // set ordering for PostStepDoIt and AtRestDoIt
      pmanager->SetProcessOrdering(theDecayProcess, idxPostStep);
      pmanager->SetProcessOrdering(theDecayProcess, idxAtRest);
    }
  }
}

void PhysicsList::ConstructEM() {
	auto theParticleIterator = GetParticleIterator();
  theParticleIterator->reset();
  while ((*theParticleIterator)()) {
    G4ParticleDefinition* particle = theParticleIterator->value();
    G4ProcessManager* pmanager = particle->GetProcessManager();
    G4String particleName = particle->GetParticleName();

    if (particleName == "gamma") {
      // gamma
      // Construct processes for gamma
      G4GammaConversion *gammaConversion = new G4GammaConversion();
      pmanager->AddDiscreteProcess(gammaConversion);
      G4ComptonScattering *comptonScattering = new G4ComptonScattering();
      pmanager->AddDiscreteProcess(comptonScattering);
      G4PhotoElectricEffect *photoElectric = new G4PhotoElectricEffect();
      pmanager->AddDiscreteProcess(photoElectric);
    } else if (particleName == "e-") {
      // electron
      // Construct processes for electron
      G4eMultipleScattering *multipleScattering = new G4eMultipleScattering();
      pmanager->AddProcess(multipleScattering, -1, 1, 1);
      G4eIonisation *ionization = new G4eIonisation();
      pmanager->AddProcess(ionization, -1, 2, 2);
      G4eBremsstrahlung *bremsstrahlung = new G4eBremsstrahlung();
      pmanager->AddProcess(bremsstrahlung, -1, 3, 3);
    } else if (particleName == "e+") {
      // positron
      // Construct processes for positron
      G4eMultipleScattering *multipleScattering = new G4eMultipleScattering();
      pmanager->AddProcess(multipleScattering, -1, 1, 1);
      G4eIonisation *ionization = new G4eIonisation();
      pmanager->AddProcess(ionization, -1, 2, 2);
      G4eBremsstrahlung *bremsstrahlung = new G4eBremsstrahlung();
      pmanager->AddProcess(bremsstrahlung, -1, 3, 3);
      G4eplusAnnihilation *annihilation = new G4eplusAnnihilation();
      pmanager->AddProcess(annihilation, 0, -1, 4);
    } else if (particleName == "mu+" || particleName == "mu-") {
      // muon
      // Construct processes for muon
      G4MuMultipleScattering *multipleScattering = new G4MuMultipleScattering();
      pmanager->AddProcess(multipleScattering, -1, 1, 1);
      G4MuIonisation *ionizatin = new G4MuIonisation();
      pmanager->AddProcess(ionizatin, -1, 2, 2);
      G4MuBremsstrahlung *bremsstralhlung = new G4MuBremsstrahlung();
      pmanager->AddProcess(bremsstralhlung, -1, 3, 3);
      G4MuPairProduction *pairProduction = new G4MuPairProduction();
      pmanager->AddProcess(pairProduction, -1, 4, 4);
    } else {
      if ((particle->GetPDGCharge() != 0.0) &&
          (particle->GetParticleName() != "chargedgeantino") &&
          !particle->IsShortLived()) {
        // all others charged particles except geantino
        G4hMultipleScattering *multipleScattering = new G4hMultipleScattering();
        pmanager->AddProcess(multipleScattering, -1, 1, 1);
        G4hIonisation *ionization = new G4hIonisation();
        pmanager->AddProcess(ionization, -1, 2, 2);
        }
    }
  }
}

void PhysicsList::ConstructOp() {
	G4Scintillation *fScintillationProcess = new G4Scintillation("Scintillation");
  G4OpAbsorption *fAbsorptionProcess = new G4OpAbsorption();
	G4OpRayleigh *fRayleighScatteringProcess = new G4OpRayleigh();
	G4OpBoundaryProcess *fBoundaryProcess = new G4OpBoundaryProcess();
	G4OpMieHG* fMieHGScatteringProcess = new G4OpMieHG();
	G4OpWLS *fWLSProcess = new G4OpWLS("OpWLS");
	//  fCerenkovProcess->DumpPhysicsTable();
  //  fScintillationProcess->DumpPhysicsTable();
  //  fRayleighScatteringProcess->DumpPhysicsTable();

  fBoundaryProcess->SetVerboseLevel(0);

  fScintillationProcess->SetScintillationYieldFactor(1.);
  fScintillationProcess->SetTrackSecondariesFirst(true);

  // Use Birks Correction in the Scintillation process

  G4EmSaturation* emSaturation = G4LossTableManager::Instance()->EmSaturation();
  fScintillationProcess->AddSaturation(emSaturation);
  auto theParticleIterator = GetParticleIterator();
  theParticleIterator->reset();
  while ((*theParticleIterator)()) {
    G4ParticleDefinition* particle = theParticleIterator->value();
    G4ProcessManager* pmanager = particle->GetProcessManager();
    G4String particleName = particle->GetParticleName();
    if (fScintillationProcess->IsApplicable(*particle)) {
      pmanager->AddProcess(fScintillationProcess);
      pmanager->SetProcessOrderingToLast(fScintillationProcess, idxAtRest);
      pmanager->SetProcessOrderingToLast(fScintillationProcess, idxPostStep);
    }
    if (particleName == "opticalphoton") {
      G4cout << " AddDiscreteProcess to OpticalPhoton " << G4endl;
      pmanager->AddDiscreteProcess(fAbsorptionProcess);
      pmanager->AddDiscreteProcess(fRayleighScatteringProcess);
      pmanager->AddDiscreteProcess(fMieHGScatteringProcess);
      pmanager->AddDiscreteProcess(fBoundaryProcess);
      pmanager->AddDiscreteProcess(fWLSProcess);
    }
  }
}
}