#ifndef CStackingAction_hh
#define CStackingAction_hh

#include "globals.hh"
#include "G4VUserTrackInformation.hh"
#include "OptMenAnalysisManager.hh"

#include "TVector3.h"

#include <string>
#include <vector>
using std::string;
using std::vector;


class G4Track;
class G4VHitsCollection;

class OptMenUserTrackInformation : public G4VUserTrackInformation
{
  public:
    OptMenUserTrackInformation();
    virtual ~OptMenUserTrackInformation();

    int getId() {return track_id;}
    void setId(int id) {track_id = id;}

  private:
    int track_id;
  };

#endif