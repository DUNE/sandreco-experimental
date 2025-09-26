#pragma once

#include "TObjString.h"
#include "TBits.h"

struct GRooTrackerEvent {
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

struct StdHep {
  int N_;
  int Pdg_;
  int Status_;
  int Rescat_;
  static const int kNPmax = 250;
  double X4_[kNPmax][4];
  double P4_[kNPmax][4];
  double Polz_[kNPmax][3];
  int Fd_[kNPmax];
  int Ld_[kNPmax];
  int Fm_[kNPmax];
  int Lm_[kNPmax];
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
    const GRooTrackerEvent& event() const {return event_;}


    void stdHep(int N, int Pdg, int Status, int Rescat,
                double X4[][4], double P4[][4], double Polz[][3],
                int Fd[], int Ld[], int Fm[], int Lm[]) {

                  stdHep_.N_ = N;
                  stdHep_.Pdg_ = Pdg;
                  stdHep_.Status_ = Status;
                  stdHep_.Rescat_ = Rescat;

                  for (int i = 0; i < N; i++) {
                      for (int j = 0; j < 4; j++) {
                          stdHep_.X4_[i][j] = X4[i][j];
                          stdHep_.P4_[i][j] = P4[i][j];
                      }
                      for (int j = 0; j < 3; j++) {
                          stdHep_.Polz_[i][j] = Polz[i][j];
                      }
                      stdHep_.Fd_[i] = Fd[i];
                      stdHep_.Ld_[i] = Ld[i];
                      stdHep_.Fm_[i] = Fm[i];
                      stdHep_.Lm_[i] = Lm[i];
                  }
                }
    const StdHep& stdHep() const {return stdHep_;}
  
  private:
    GRooTrackerEvent event_;
    StdHep stdHep_;
};