//---------------------------------------------------------------------------//
#include "OptMenReadParameters.hh"  
#include <fstream>
#include <sstream>
#include "G4SystemOfUnits.hh"
#include <CLHEP/Units/SystemOfUnits.h>
#include <cmath>
#include <math.h>
#include "G4PhysicalConstants.hh"
#include "Randomize.hh"

using namespace std;

OptMenReadParameters* OptMenReadParameters::me = 0;

// singleton
OptMenReadParameters::OptMenReadParameters(){

	//defalut values
	fTemporaryFiles = false;
	fOpticalPhotonsFile = false;
	fOpticalPhotonsName = "opticalPhotons.root";
	fPrimariesFile = false;
 	fPrimariesName = "primaries.root";
	fSensorsFile = false;
	fSensorsName = "sensor.root";
	fSensorsTreeFile = false;
	fSensorsTreeName = "";
 	fInputFile = "run_mu.mac";
	fGeometryFile = "main.gdml";
	fLensType = false;
	fXeDoping = false;
	fUI = false;
	fStartingEntry = 0;
	fDestinationPath = "./";
	fStartingOnly = false;
	fFullVolume = true;
	fEnergySplitThreshold = 100;
	fIDlistFile = false;
	fIDlistName = ""; 	

	//default values are updated in OptMen.cc by calling ReadConfigurationFile or
	//by setting values later with class Messengers using Set methods	
}

OptMenReadParameters* OptMenReadParameters::Get() {
	if (!me)
		me = new  OptMenReadParameters();
	return me;
}


void OptMenReadParameters::ReadConfigurationFile(ifstream& ifs) {

    std::cout << "****** READING CONFIGURATION FILE ******" << std::endl;	

    if (ifs) {
      
				std::string line;
      	while(getline(ifs, line)) {
        
		//skip empty lines or comments
		if(line.empty() || line[0] == '#') {
          		continue;
       		 }
       
		std::cout << line << std::endl;

	 	auto delimiterPos = line.find("=");
        auto name = line.substr(0, delimiterPos);
        auto value = line.substr(delimiterPos + 1);

		if(name.find("opticalPhotonsFile") != std::string::npos){
			if(value.find("yes") != std::string::npos) fOpticalPhotonsFile = true;
		}
		if(name.find("opticalPhotonsName") != std::string::npos){
			while (value.front() == ' ') value.erase(value.begin());
			if(fOpticalPhotonsFile && value != "") fOpticalPhotonsName = value;
		}
		if(name.find("primariesFile") != std::string::npos){
			if(value.find("yes") != std::string::npos) fPrimariesFile = true;
		}
		if(name.find("primariesName") != std::string::npos){
			while (value.front() == ' ') value.erase(value.begin());
			if(fPrimariesFile && value != "") fPrimariesName = value;
		}
		if(name.find("sensorsFile") != std::string::npos){
			if(value.find("yes") != std::string::npos) fSensorsFile = true;
		}
		if(name.find("sensorsName") != std::string::npos){
			while (value.front() == ' ') value.erase(value.begin());
			if(fSensorsFile && value != "") fSensorsName = value;
		}
		if(name.find("geometryFile") != std::string::npos){
			while (value.front() == ' ') value.erase(value.begin());
			if (value != "") fGeometryFile = value;
		}
		if(name.find("ui") != std::string::npos){
			if(value.find("yes") != std::string::npos) fUI = true;
		}
		if(name.find("destinationPath") != std::string::npos){
			while (value.front() == ' ') value.erase(value.begin());
			if(value != "") fDestinationPath = value;
		}
		if(name.find("inputFile") != std::string::npos){
			if(value != "") fInputFile = value;
		}
		if(name.find("generatorType") != std::string::npos){
			if(value != "") fGeneratorType = value;
		}
		if(name.find("eventNumber") != std::string::npos){
			if(value != "") fEventNumber = std::stoi(value);
		}
		if(name.find("temporaryFiles") != std::string::npos){
			if(value.find("yes") != std::string::npos) fTemporaryFiles = true;
		}
		if(name.find("startingEntry") != std::string::npos){
			if(value != "") fStartingEntry = std::stoi(value);
		}
		if(name.find("startingOnly") != std::string::npos){
			if(value.find("yes") != std::string::npos) fStartingOnly = true;
		}
		if(name.find("fullVolume") != std::string::npos){
			if(value.find("no") != std::string::npos) fFullVolume = false;
		}
		if(name.find("energySplitThreshold") != std::string::npos){
			if(value != "") fEnergySplitThreshold = std::stod(value);
		}
		if(name.find("IDlistFile") != std::string::npos){
			if(value.find("yes") != std::string::npos) fIDlistFile = true;
		}
		if(name.find("IDlistName") != std::string::npos){
			while (value.front() == ' ') value.erase(value.begin());
			if(fIDlistFile && value != "") fIDlistName = value;
		}
	}

	if( fGeometryFile.find("lenses") != string::npos ){
		fLensType = true;
		std::cout << "Lenses detected!!" << std::endl;
	}
	if( fGeometryFile.find("Xe") != string::npos ){
		fXeDoping = true;
		std::cout << "Xe-doping detected!!" << std::endl;	
	}
     }

	std::cout << "Input configuration file loaded" << std::endl;
	
}

