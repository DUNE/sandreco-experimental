#pragma once

#include "TObjString.h"
#include "TBits.h"

struct gRooTrackerEvent {
  int EvtNum_;
  double EvtXSec_;
  double EvtDXSec_;
  double EvtKPS_;
  double EvtWght_;
  double EvtProb_;
  double* EvtVtx_;
  TObjString* EvtCode_;
  TBits* EvtFlags_;
};

class GenieWrapper {
  public:
    GenieWrapper() {};    
 
    void event(int EvtNum, double EvtXSec, double EvtDXSec, 
               double EvtKPS, double EvtWght, double EvtProb,
               double* EvtVtx, TObjString* EvtCode, TBits* EvtFlags) {

                event_.EvtNum_ = EvtNum;
                event_.EvtXSec_ = EvtXSec;
                event_.EvtDXSec_ = EvtDXSec;
                event_.EvtKPS_ = EvtKPS;
                event_.EvtWght_ = EvtWght;
                event_.EvtProb_ = EvtProb;
                event_.EvtVtx_ = EvtVtx;
                event_.EvtCode_ = EvtCode;
                event_.EvtFlags_ = EvtFlags;

               };
    const gRooTrackerEvent& event() const {return event_;}
    

  private:
    gRooTrackerEvent event_;
};