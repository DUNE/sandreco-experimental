#ifndef CStackingAction_hh
#define CStackingAction_hh

#include "globals.hh"
#include "G4UserStackingAction.hh"
#include "OptMenAnalysisManager.hh"
#include "OptMenEventData.hh"

#include "TTree.h"
#include "TFile.h"
#include "TVector3.h"

#include <string>
#include <vector>
using std::string;
using std::vector;


class G4Track;
class G4VHitsCollection;

class OptMenStackingAction : public G4UserStackingAction
{
  public:
    OptMenStackingAction(OptMenAnalysisManager* mgr);
    virtual ~OptMenStackingAction();

    virtual G4ClassificationOfNewTrack ClassifyNewTrack(const G4Track* aTrack);
    virtual void NewStage();
    virtual void PrepareNewEvent();

  private:

    OptMenAnalysisManager* _anMgr;
    OptMenEventData* sData;
  };

#endif