#ifndef OptMenPHYSICSLIST_H
#define OptMenPHYSICSLIST_H

#include <G4EmStandardPhysics.hh>
#include <G4VModularPhysicsList.hh>
#include "globals.hh"

#include "G4Cerenkov.hh"
#include "G4OpAbsorption.hh"
#include "G4OpBoundaryProcess.hh"
#include "G4OpMieHG.hh"
#include "G4OpRayleigh.hh"
#include "G4OpWLS.hh"
#include "G4Scintillation.hh"

class G4Cerenkov;
class G4Scintillation;
class G4OpBoundaryProcess;
class G4OpBoundary;
class G4OpAbsorption;
class G4OpRayleigh;
class G4OpWLS;
class G4OpMieHG;

class OptMenPhysicsList : public G4VModularPhysicsList {
 private:
	 
 public:
  OptMenPhysicsList();
  virtual ~OptMenPhysicsList();
  virtual void setCuts();

  virtual void ConstructParticle();
  virtual void ConstructProcess();
  void ConstructEM();
  void ConstructOp();
  void ConstructGeneral();
};

#endif /* OptMenPHYSICSLIST_H */
