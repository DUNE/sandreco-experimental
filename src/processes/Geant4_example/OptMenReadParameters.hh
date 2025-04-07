// --------------------------------------------------------------------------//

#ifndef OPTMENREADPARAMETERSH_H
#define OPTMENREADPARAMETERSH_H

#include "G4ThreeVector.hh"
#include "G4SystemOfUnits.hh"
#include <vector>
#include "math.h"
#include <iostream>
#include <string>
#include <fstream>

using namespace std;

////////////////////////////////////////////////////////
///  This class reads parameters from files and stores them
///  It can be called form any other class to retrieve 
///  configuration option or parameters whenever needed
////////////////////////////////////////////////////////////

class OptMenReadParameters  {
	private:
		OptMenReadParameters();
	public:
		static OptMenReadParameters* Get();

		virtual ~OptMenReadParameters() {}

		///It reads the configuration file
		void ReadConfigurationFile(ifstream& ifs);
		////////////////////////////////////////////////////
		
		void SetOpticalPhotonsFile(G4bool value) { fOpticalPhotonsFile= value;}
		G4bool GetOpticalPhotonsFile() { return fOpticalPhotonsFile;}
		void SetOpticalPhotonsName(G4String value) { fOpticalPhotonsName= value;}
		G4String GetOpticalPhotonsName() { return fOpticalPhotonsName;}
		
		void SetIDlistFile(G4bool value) { fIDlistFile= value;}
		G4bool GetIDlistFile() { return fIDlistFile;}
		void SetIDlistName(G4String value) { fIDlistName= value;}
		G4String GetIDlistName() { return fIDlistName;}

		void SetIdList(std::vector<int> value) { idList= value;}
		void pushIdList(int value) {idList.push_back(value);}
		std::vector<int> GetIdList() { return idList;}

		G4bool GetTemporaryFiles() { return fTemporaryFiles;}

		void SetPrimariesFile(G4bool value) { fPrimariesFile= value;}
		G4bool GetPrimariesFile() { return fPrimariesFile;}
		void SetPrimariesName(G4String value) { fPrimariesName= value;}
		G4String GetPrimariesName() { return fPrimariesName;}
	
		void SetSensorsFile(G4bool value) { fSensorsFile= value;}
		G4bool GetSensorsFile() { return fSensorsFile;}
		void SetSensorsName(G4String value) { fSensorsName= value;}
		G4String GetSensorsName() { return fSensorsName;}

		void SetGeometryFile(G4String value) { fGeometryFile= value;}
		G4String GetGeometryFile() { return fGeometryFile;}
		G4bool GetLensType() { return fLensType;}
		G4bool GetXeDoping() { return fLensType;}

		void SetUI(G4bool value) { fUI= value;}
		G4bool GetUI() { return fUI;}
		
		void SetDestinationPath(G4String value) { fDestinationPath = value;}
		G4String GetDestinationPath() { return fDestinationPath;}
		void SetInputFile(G4String value) { fInputFile= value;}
		G4String GetInputFile() { return fInputFile;}
		void SetGeneratorType(G4String value) { fGeneratorType = value;}
		G4String GetGeneratorType() { return fGeneratorType;}

		void SetEventNumber(G4int value) { fEventNumber= value;}
		G4int GetEventNumber() { return fEventNumber;}
		void SetStartingEntry(G4int value) { fStartingEntry= value;}
		G4int GetStartingEntry() { return fStartingEntry;}

		void SetSplitEventNumber(G4int value) { fSplitEventNumber= value;}
		G4int GetSplitEventNumber() { return fSplitEventNumber;}
		void SetEnergySplitThreshold(G4double value) {fEnergySplitThreshold = value;}
		G4double GetEnergySplitThreshold() {return fEnergySplitThreshold;}

		void SetSensorsTreeFile(G4bool value) { fSensorsTreeFile= value;}
		G4bool GetSensorsTreeFile() { return fSensorsTreeFile;}
		
		void SetFullVolume(G4bool value) { fFullVolume= value;}
		G4bool GetFullVolume() { return fFullVolume;}

		void SetSensorsTreeName(std::vector<G4String> value) { sensorsTreeName= value;}
		void pushSensorsTreeName(G4String value) {sensorsTreeName.push_back(value);}
		std::vector<G4String> GetSensorsTreeName() { return sensorsTreeName;}
	
		void SetStartingOnly(G4bool value) { fStartingOnly= value;}
		G4bool GetStartingOnly() { return fStartingOnly;}

		void SetGeometryHash(G4String value) { fGeometryHash= value;}
		G4String GetGeometryHash() { return fGeometryHash;}

		void SetEDepSimEvtIdFromEntry(G4int entry, G4int eventID) { fEDepSimEvtId.insert(std::make_pair(entry,eventID));}
		G4int GetEDepSimEvtIdFromEntry(G4int entry) { return fEDepSimEvtId[entry]; }
	
	
	private:
		//the singleton
		static OptMenReadParameters *me;

		//input/output options
		G4bool fOpticalPhotonsFile;
		G4String fOpticalPhotonsName;

		G4bool	fTemporaryFiles;

		G4bool	fPrimariesFile;
  		G4String fPrimariesName;
  
		G4bool fSensorsFile;
  		G4String fSensorsName;

		G4bool fSensorsTreeFile;
			G4String fSensorsTreeName;
  
		G4String fGeometryFile;
		G4bool fLensType;
		G4bool fXeDoping;		


		G4bool fIDlistFile;
		G4String fIDlistName;
		std::vector<int> idList;

		G4String fDestinationPath;
  		G4String fInputFile;
  		G4String fGeneratorType;
		G4int fEventNumber;
		G4int fStartingEntry;
		G4bool fStartingOnly;
		G4int fSplitEventNumber;
		G4double fEnergySplitThreshold;
		G4int fFullVolume;
		G4bool fUI;

		G4String fGeometryHash;

		//Association btw file entry and event id
		std::map<G4int,G4int> fEDepSimEvtId;

		std::vector<G4String> sensorsTreeName;


};



#endif
