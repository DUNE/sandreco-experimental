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
  fFileName = OptMenReadParameters::Get()->GetInputFile();
  requestedEvents = OptMenReadParameters::Get()->GetEventNumber();
  startingEntry = OptMenReadParameters::Get()->GetStartingEntry();
  energySplitThreshold = OptMenReadParameters::Get()->GetEnergySplitThreshold();
  eventCount = 0;
  isArgon = false;
}

OptMenEventCounter::~OptMenEventCounter() {}

void OptMenEventCounter::ReadEDepSimEvent() {}

void OptMenEventCounter::CountEvents() {
	fNHits = 0;
	fTotEnDep = 0;
	fTotSecondaryEnDep = 0;

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
}

void OptMenEventCounter::ReadGenieEvent() {
	// fInput = new TFile(fFileName.c_str(),"READ");
	// if (!fInput->IsOpen()) {
	// 	std::cout << "ERROR : " << fFileName << " cannot be opened! "<< std::endl;
	// 	exit(EXIT_FAILURE);
	// }

	// TTree* fTree = (TTree*)fInput->Get("gRooTracker");
  // 	if (!fTree) {
	// 	std::cout << "ERROR : Tree not found." << std::endl;
	// 	exit(EXIT_FAILURE);
	// }
	// std::cout << "Opening the GENIE file in a RooTracker tree:  " << fFileName <<std::endl;

	// std::cout<< "File has " << fTree->GetEntries() << " entries."<<std::endl;

  // if (OptMenReadParameters::Get()->GetStartingOnly() == false) {
	//   if (startingEntry + requestedEvents > fTree->GetEntries()) {
  // 		std::cout << "Requested " << requestedEvents << " events starting from entry number " << startingEntry <<std::endl;
	//   	std::cout << "but the file has only " << fTree->GetEntries() << " entries." <<std::endl;

	// 	requestedEvents = fTree->GetEntries() - startingEntry;
	// 	std::cout << "Changing the number of requested events to: " << requestedEvents << std::endl;
	//   }
  // }
  // eventCount = requestedEvents;
  // fInput->Close();
}
int OptMenEventCounter::GetEventsCount() {
	if (OptMenReadParameters::Get()->GetGeneratorType().find("edepsim") != std::string::npos) {
		CountEvents();
	}
  if (OptMenReadParameters::Get()->GetGeneratorType().find("genie") != std::string::npos) {
		ReadGenieEvent();
	}
  std::cout << eventCount << std::endl;
  return eventCount;
}
