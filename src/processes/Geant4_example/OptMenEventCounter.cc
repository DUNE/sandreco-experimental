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
/// \file OptMenEventCounter.cc
/// \brief Implementation of the OptMenEventCounter class

#include "OptMenEventCounter.hh"

#include <EdepReader/EdepReader.hpp>
#include <ufw/context.hpp>

#include "G4Event.hh"
#include "G4EventManager.hh"
#include "G4TrajectoryContainer.hh"
#include "G4Trajectory.hh"
#include "G4ios.hh"

OptMenEventCounter::OptMenEventCounter()
{
  energySplitThreshold = OptMenReadParameters::Get()->GetEnergySplitThreshold();
	fTotSecondaryEnDep = 0;
  eventCount = 0;
	fTotEnDep = 0;
	fNHits = 0;
  isArgon = false;
}

OptMenEventCounter::~OptMenEventCounter() {}

int OptMenEventCounter::GetEventsCount() {
	auto& tree = ufw::context::instance<sand::EdepReader>();

	for (auto trj_it = tree.begin(); trj_it != tree.end(); trj_it++) {

		if (trj_it->GetHitMap().find(component::GRAIN) != trj_it->GetHitMap().end()) {
			
			isArgon = true;

			for (const auto& hit : trj_it->GetHitMap().at(component::GRAIN)) {
				fTotEnDep += hit.GetEnergyDeposit();
				fTotSecondaryEnDep += hit.GetSecondaryDeposit();
				fNHits += 1;
				if (fTotEnDep > energySplitThreshold) {
					std::cout << "Reached " <<  fTotEnDep << " MeV. Adding a new event." << std::endl;
					eventCount++;
					fTotEnDep = 0;
				}
			}
    }

		if (fTotEnDep != 0 && std::next(trj_it) == tree.end()) {
			std::cout << "Reached end of event at " <<  fTotEnDep << " MeV. Adding a new event." << std::endl;
			eventCount++;
		}
	}

	if(!isArgon) {
		std::cout << "Reached end of event with no argon hits." << std::endl;
		eventCount++;
	} else {
		isArgon = false;
	}

	std::cout << eventCount << std::endl;
	return eventCount;
}