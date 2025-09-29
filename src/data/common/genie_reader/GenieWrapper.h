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

  StdHep() = default;
  
  StdHep(int N, int Pdg, int Status, int Rescat,
         double X4[][4], double P4[][4], double Polz[][3],
         int Fd[], int Ld[], int Fm[], int Lm[]) 
         : N_(N), Pdg_(Pdg), Status_(Status), Rescat_(Rescat),
         X4_(N, std::vector<double>(4)),
         P4_(N, std::vector<double>(4)),
         Polz_(N, std::vector<double>(3)),
         Fd_(N), Ld_(N), Fm_(N), Lm_(N)
        {
          for (int i = 0; i < N; i++) {
            for (int j = 0; j < 4; j++) {
              X4_[i][j] = X4[i][j];
              P4_[i][j] = P4[i][j];
            }
            for (int j = 0; j < 3; j++) {
                Polz_[i][j] = Polz[i][j];
            }
            Fd_[i] = Fd[i];
            Ld_[i] = Ld[i];
            Fm_[i] = Fm[i];
            Lm_[i] = Lm[i];
          }
        }

    int N_;
    int Pdg_;
    int Status_;
    int Rescat_;

    std::vector<std::vector<double>> X4_;
    std::vector<std::vector<double>> P4_;
    std::vector<std::vector<double>> Polz_;

    std::vector<int> Fd_;
    std::vector<int> Ld_;
    std::vector<int> Fm_;
    std::vector<int> Lm_;
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

struct GenieWrapper {
  GRooTrackerEvent event_;
  StdHep stdHep_;
  NumiFlux numiFlux_;
};