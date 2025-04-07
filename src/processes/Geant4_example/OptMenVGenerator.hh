#ifndef _BXVGENERATOR_HH
#define _BXVGENERATOR_HH

#include "globals.hh"
#include "G4ThreeVector.hh"

class G4Event;
class G4UImessenger;
class G4Run;

/// This is a general generator class from which other implementations are derived
/// It allows easy switch between generators, because its methods can be called
/// uniquely in OptMenPrimaryGeneratorAction but are overloaded in each implementation

class OptMenVGenerator {
	public:

		//default constructor
		OptMenVGenerator(const G4String &myname);

		///destructor
		virtual ~OptMenVGenerator();

		//public interface
		G4String GetGeneratorName() { return fGeneratorName; }
		void SetReportingFrequency(G4int freq) { fReportingFrequency = freq; }

		/// Public interface to GeneratePrimaries
		/// This method get overloaded by each specific generator class
		virtual void GeneratePrimaries(G4Event *event) = 0;

	private:
		G4String        fGeneratorName;  // Name of Generator.

		//protected members
	protected:
		G4UImessenger*   fG4Messenger;   // G4Messenger for setting up generator.
		G4int            fReportingFrequency; // Generate report every fReportingFrequency events;  
};
#endif
