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

struct NumiFlux {
  int Run_;
  int Evtno_;
  double Ndxdz_;
  double Ndydz_;
  double Npz_;
  double Nenergy_;
  double Ndxdznea_;
  double Ndydznea_;
  double Nenergyn_;
  double Nwtnear_;
  double Ndxdzfar_;
  double Ndydzfar_;
  double Nenergyf_;
  double Nwtfar_;
  int Norig_;
  int Ndecay_;
  int Ntype_;
  double Vx_;
  double Vy_;
  double Vz_;
  double Pdpx_;
  double Pdpy_;
  double Pdpz_;
  double Ppdxdz_;
  double Ppdydz_;
  double Pppz_;
  double Ppenergy_;
  int Ppmedium_;
  int Ptype_;
  double Ppvx_;
  double Ppvy_;
  double Ppvz_;
  double Muparpx_;
  double Muparpy_;
  double Muparpz_;
  double Mupare_;
  double Necm_;
  double Nimpwt_;
  double Xpoint_;
  double Ypoint_;
  double Zpoint_;
  double Tvx_;
  double Tvy_;
  double Tvz_;
  double Tpx_;
  double Tpy_;
  double Tpz_;
  double Tptype_;
  double Tgen_;
  double Tgptype_;
  double Tgppx_;
  double Tgppy_;
  double Tgppz_;
  double Tprivx_;
  double Tprivy_;
  double Tprivz_;
  double Beamx_;
  double Beamy_;
  double Beamz_;
  double Beampx_;
  double Beampy_;
  double Beampz_;
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

    void numiFlux(int Run, int Evtno, double Ndxdz, double Ndydz, 
                  double Npz, double Nenergy, double Ndxdznea, 
                  double Ndydznea, double Nenergyn, double Nwtnear, 
                  double Ndxdzfar, double Ndydzfar, double Nenergyf, 
                  double Nwtfar, int Norig, int Ndecay, int Ntype, 
                  double Vx, double Vy, double Vz, double Pdpx, 
                  double Pdpy, double Pdpz, double Ppdxdz, double Ppdydz, 
                  double Pppz, double Ppenergy, int Ppmedium, int Ptype, 
                  double Ppvx, double Ppvy, double Ppvz, double Muparpx, 
                  double Muparpy, double Muparpz, double Mupare, 
                  double Necm, double Nimpwt, double Xpoint, double Ypoint, 
                  double Zpoint, double Tvx, double Tvy, double Tvz, 
                  double Tpx, double Tpy, double Tpz, double Tptype, 
                  double Tgen, double Tgptype, double Tgppx, double Tgppy, 
                  double Tgppz, double Tprivx, double Tprivy, double Tprivz, 
                  double Beamx, double Beamy, double Beamz, double Beampx, 
                  double Beampy, double Beampz) {

                    numiFlux_.Run_ = Run;
                    numiFlux_.Evtno_ = Evtno;
                    numiFlux_.Ndxdz_ = Ndxdz;
                    numiFlux_.Ndydz_ = Ndydz;
                    numiFlux_.Npz_ = Npz;
                    numiFlux_.Nenergy_ = Nenergy;
                    numiFlux_.Ndxdznea_ = Ndxdznea;
                    numiFlux_.Ndydznea_ = Ndydznea;
                    numiFlux_.Nenergyn_ = Nenergyn;
                    numiFlux_.Nwtnear_ = Nwtnear;
                    numiFlux_.Ndxdzfar_ = Ndxdzfar;
                    numiFlux_.Ndydzfar_ = Ndydzfar;
                    numiFlux_.Nenergyf_ = Nenergyf;
                    numiFlux_.Nwtfar_ = Nwtfar;
                    numiFlux_.Norig_ = Norig;
                    numiFlux_.Ndecay_ = Ndecay;
                    numiFlux_.Ntype_ = Ntype;
                    numiFlux_.Vx_ = Vx;
                    numiFlux_.Vy_ = Vy;
                    numiFlux_.Vz_ = Vz;
                    numiFlux_.Pdpx_ = Pdpx;
                    numiFlux_.Pdpy_ = Pdpy;
                    numiFlux_.Pdpz_ = Pdpz;
                    numiFlux_.Ppdxdz_ = Ppdxdz;
                    numiFlux_.Ppdydz_ = Ppdydz;
                    numiFlux_.Pppz_ = Pppz;
                    numiFlux_.Ppenergy_ = Ppenergy;
                    numiFlux_.Ppmedium_ = Ppmedium;
                    numiFlux_.Ptype_ = Ptype;
                    numiFlux_.Ppvx_ = Ppvx;
                    numiFlux_.Ppvy_ = Ppvy;
                    numiFlux_.Ppvz_ = Ppvz;
                    numiFlux_.Muparpx_ = Muparpx;
                    numiFlux_.Muparpy_ = Muparpy;
                    numiFlux_.Muparpz_ = Muparpz;
                    numiFlux_.Mupare_ = Mupare;
                    numiFlux_.Necm_ = Necm;
                    numiFlux_.Nimpwt_ = Nimpwt;
                    numiFlux_.Xpoint_ = Xpoint;
                    numiFlux_.Ypoint_ = Ypoint;
                    numiFlux_.Zpoint_ = Zpoint;
                    numiFlux_.Tvx_ = Tvx;
                    numiFlux_.Tvy_ = Tvy;
                    numiFlux_.Tvz_ = Tvz;
                    numiFlux_.Tpx_ = Tpx;
                    numiFlux_.Tpy_ = Tpy;
                    numiFlux_.Tpz_ = Tpz;
                    numiFlux_.Tptype_ = Tptype;
                    numiFlux_.Tgen_ = Tgen;
                    numiFlux_.Tgptype_ = Tgptype;
                    numiFlux_.Tgppx_ = Tgppx;
                    numiFlux_.Tgppy_ = Tgppy;
                    numiFlux_.Tgppz_ = Tgppz;
                    numiFlux_.Tprivx_ = Tprivx;
                    numiFlux_.Tprivy_ = Tprivy;
                    numiFlux_.Tprivz_ = Tprivz;
                    numiFlux_.Beamx_ = Beamx;
                    numiFlux_.Beamy_ = Beamy;
                    numiFlux_.Beamz_ = Beamz;
                    numiFlux_.Beampx_ = Beampx;
                    numiFlux_.Beampy_ = Beampy;
                    numiFlux_.Beampz_ = Beampz;
                  }
    const NumiFlux& numiFlux() const {return numiFlux_;}
  private:
    GRooTrackerEvent event_;
    StdHep stdHep_;
    NumiFlux numiFlux_;
};