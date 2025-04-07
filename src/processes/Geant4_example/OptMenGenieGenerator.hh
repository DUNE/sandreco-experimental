// --------------------------------------------------------------------------//

#ifndef _DUROOTRACKERGENERATOR_HH
#define _DUROOTRACKERGENERATOR_HH

//---------------------------------------------------------------------------//
#include "TTree.h"
#include "TFile.h"
#include "G4ThreeVector.hh"
#include "Randomize.hh"
#include "G4ParticleGun.hh"
#include "OptMenVGenerator.hh"
#include "G4Event.hh"
#include "G4Box.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4String.hh"
#include "TBits.h"
#include "TMatrixD.h"
#include <G4SPSPosDistribution.hh>
//---------------------------------------------------------------------------//
///RooTracker generator: generates events read from GENIE output file
class OptMenGenieGenerator : public OptMenVGenerator {
	public:

		///default constructor
		OptMenGenieGenerator();

		///destructor
		virtual ~OptMenGenieGenerator();

		///public interface
		virtual void GeneratePrimaries(G4Event *event);
    void ReadTree();

		void SetFileName (TString f){ fFileName = f;} 
		void SetUniformPositionDistribution(bool f){fUniformDistr=f;}
		///Used to confine the start positions to a particular volume.
		void ConfineSourceToVolume(G4String vol){ fSPSPos->ConfineSourceToVolume(vol); }
		void SetIfVolDist(G4bool a) { fVolumeFlag = a; }
		void SetParticlePosition(G4ThreeVector pos){fParticleGun->SetParticlePosition(pos); }
		void SetGenieTreeEntry(int a) { fEntry = a; }
		TMatrixD rotateVector(double x, double y, double z, double angX, double angY, double angZ);

		//private  members
	private:

		// DuRooTrackerGeneratorMessenger*   fTheMessenger;
		G4ParticleGun*               fParticleGun;
		G4ParticleTable*             fParticleTable;
		G4bool                       fIsFirstTime ;
		G4bool                       fIsLastTime ;

		TFile* fInput;   // The RooTracker file to read.
		TTree* fTree;    //Tree degli eventi di Genie
		TString fFileName;

		int fEntry;  //index running on entries of the tree file
			     //currently fixed to value chosen via mac file
		bool fUniformDistr;
		G4SPSPosDistribution*        fSPSPos;
		G4bool                       fVolumeFlag ;

		//////////////////////////////////////////////////////////////
		// Declare the information to get from the ntuple.  This does not get all
		// of the information, only the stuff that is actually used.
		//////////////////////////////////////////////////////////////

		/// The generator-specific event flags.
		TBits*       fEvtFlags;

		/// The generator-specific string with the 'event code'
		G4String*  fEvtCode;

		/// The sequence number of the event (the event number).
		int         fEvtNum;

		/// The cross section for the event (1E-38 cm2)
		double      fEvtXSec;

		/// The differential cross section for the event kinematics
		/// (1E-38 cm2/{K^n})
		double      fEvtDXSec;

		/// The weight for the event
		double      fEvtWght;

		/// The probability for the event (given the cross section, path lengths,
		/// etc.).
		double      fEvtProb;

		/// The event vertex position in detector coord syst (in meters and
		/// seconds).

		double      fEvtVtx[4];

		/// The number of particles in the particle arrays to track
		int         fStdHepN;

		/// The maximum number of particles that can be in the particle arrays.
		static const int kNPmax = 4000;

		/// The PDG codes for the particles to track.  This may include generator
		/// specific codes for pseudo particles.
		int         fStdHepPdg[kNPmax]; //[fStdHepN]

		/// The a generator specific status for each particle.  Particles with a
		/// status equal to 1 will be tracked.
		///
		/// The HEPEVT status values are as follows:
		/// - 0 : null entry.
		/// - 1 : an existing entry, which has not decayed or fragmented. This is
		///    the main class of entries, which represents the `final state' given
		///    by the generator.
		/// - 2 : an entry which has decayed or fragmented and is therefore not
		///    appearing in the final state, but is retained for event history
		///    information.
		/// - 3 : a documentation line, defined separately from the event
		///    history. This could include the two incoming reacting particles,
		///    etc.
		/// - 4 to 10 :
		///    undefined, but reserved for future standards.
		/// - 11 to 200 : at the disposal of each model builder for constructs
		///    specific to his program, but equivalent to a null line in the
		///    context of any other program.
		/// - 201 and above : at the disposal of users, in particular for event
		/// tracking in the detector.

		/// The GENIE generator approximately follows the HEPEVT status codes.
		/// As of July 2008, the status values found the GENIE source code are:
		///   - -1 -- Undefined particle
		///   -  0 -- An initial state particle.
		///   -  1 -- A stable final state particle to be tracked.
		///   -  2 -- An intermediate particle that should not be tracked.
		///   -  3 -- A particle which decayed and should not be tracked.  If
		///            this particle produced daughters to be tracked, they will
		///            have a state of 1.
		int         fStdHepStatus[kNPmax]; //[fStdHepN]

		/// The position (x, y, z, t) (fm, second) of the particle in the nuclear
		/// frame
		double      fStdHepX4[kNPmax][4]; //[fStdHepN]

		/// The 4-momentum (px, py, pz, E) of the particle in the LAB frame (GeV)
		double      fStdHepP4[kNPmax][4]; //[fStdHepN]

		/// The particle polarization vector.
		double      fStdHepPolz  [kNPmax][3]; //[fStdHepN]

		/// The index of the first daughter of the particle in the arrays.
		int         fStdHepFd[kNPmax]; //[fStdHepN]

		/// The index last daughter of the particle in the arrays.
		int         fStdHepLd[kNPmax]; //[fStdHepN]

		/// The index of the first mother of the particle in there arrays.
		int         fStdHepFm[kNPmax]; //[fStdHepN]

		/// The index of the last mother of the particle in the arrays.
		int         fStdHepLm[kNPmax]; //[fStdHepN]

		//////////////////////////////
		/// The following variables are copied more or less directly from the
		/// input flux generator.
		//////////////////////////////

		/// The PDG code of the particle which created the parent neutrino.
		int         fNuParentPdg;

		/// The interaction mode at the vertex which created the parent neutrino.
		/// This is normally the decay mode of the parent particle.
		int         fNuParentDecMode;

		/// The 4 momentum of the particle at the vertex which created the parent
		/// neutrino.  This is normally the momentum of the parent particle at the
		/// decay point.
		double      fNuParentDecP4[4];

		/// The position of the vertex at which the neutrino was created.  This is
		/// passed directly from the beam (or other flux) generator, and is in the
		/// native units of the original generator.
		double      fNuParentDecX4[4];

		/// The momentum of the parent particle at it's production point.  This is
		/// in the native energy units of the flux generator.
		double      fNuParentProP4[4];

		/// The position of the parent particle at it's production point.  This
		/// uses the target as the origin and is in the native units of the flux
		/// generator.
		double      fNuParentProX4[4];

		/// The vertex ID of the parent particle vertex.
		int         fNuParentProNVtx;

		// Event code as per http://wng.ift.uni.wroc.pl/karp45/software/NeutUsage.pdf
		int fG2NeutEvtCode;

    int startingEntry;
    int nEvents;

  	G4Box* fVol;

    static int eventIndex;
    static int eventCount;


};
#endif
