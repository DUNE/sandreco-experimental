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
/// \file OptMenPrimaryGeneratorAction.hh
/// \brief Definition of the OptMenPrimaryGeneratorAction class

#ifndef OptMenPrimaryGeneratorAction_h
#define OptMenPrimaryGeneratorAction_h 1

#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4GeneralParticleSource.hh"
#include "OptMenVGenerator.hh"
#include "globals.hh"

class G4Event;

/// The primary generator action class is used to switch between generators
/// Instead of implmenting the generator, it builds a generic one (OptMenVGenerator)
/// which is the base class of all implementations (its methods are overloaded)

class OptMenPrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction
{
  public:
    OptMenPrimaryGeneratorAction();    
    virtual ~OptMenPrimaryGeneratorAction();

    virtual void GeneratePrimaries(G4Event* );

    void SetGenerator(OptMenVGenerator* gen) { fGenerator = gen;}
    OptMenVGenerator* GetGenerator() const { return fGenerator;}

  private:

    OptMenVGenerator *fGenerator; //generic generator 

};
#endif
