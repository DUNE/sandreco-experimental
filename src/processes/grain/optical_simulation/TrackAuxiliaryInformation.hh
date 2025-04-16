#ifndef CStackingAction_hh
#define CStackingAction_hh

#include "globals.hh"
#include "G4VUserTrackInformation.hh"
#include "AnalysisManager.hh"

#include "TVector3.h"

#include <string>
#include <vector>
using std::string;
using std::vector;


class G4Track;
class G4VHitsCollection;

class UserTrackInformation : public G4VUserTrackInformation
{
  public:
    UserTrackInformation();
    virtual ~UserTrackInformation();

    int getId() {return track_id;}
    void setId(int id) {track_id = id;}

  private:
    int track_id;
  };

#endif