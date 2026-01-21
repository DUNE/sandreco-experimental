#ifndef CStackingAction_hh
#define CStackingAction_hh

#include "globals.hh"
#include "G4UserStackingAction.hh"
#include "AnalysisManager.hh"


#include <string>
#include <vector>
using std::string;
using std::vector;


class G4Track;
class G4VHitsCollection;

namespace sand::grain {
class StackingAction : public G4UserStackingAction
{
  public:
    StackingAction(AnalysisManager* mgr);
    virtual ~StackingAction();

    virtual G4ClassificationOfNewTrack ClassifyNewTrack(const G4Track* aTrack);
    virtual void NewStage();
    virtual void PrepareNewEvent();

  private:
    AnalysisManager* _anMgr;
};
}
#endif