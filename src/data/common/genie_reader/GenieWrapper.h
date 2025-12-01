#pragma once

#include <Math/Vector3D.h>
#include <Math/Vector4D.h>
#include <TBits.h>
#include <TObjString.h>

#include <optional>
#include <vector>

struct GRooTrackerEvent {
  GRooTrackerEvent() = default;

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

enum class StdHepIndex : int { nu = 0, tgt = 1 };

struct StdHep {
  StdHep() = default;

  StdHep(int N, const int Pdg[], const int Status[], const int Rescat[], const double X4[][4], const double P4[][4],
         const double Polz[][3], const int Fd[], const int Ld[], const int Fm[], const int Lm[]);

  [[nodiscard]] std::vector<int> daughters_indexes_of_part(int particle_index) const;

  int N_{};
  std::vector<int> Pdg_{};
  std::vector<int> Status_{};
  std::vector<int> Rescat_{};

  std::vector<ROOT::Math::XYZTVector> X4_{};
  std::vector<ROOT::Math::PxPyPzEVector> P4_{};
  std::vector<ROOT::Math::XYZVector> Polz_{};

  std::vector<int> Fd_{};
  std::vector<int> Ld_{};
  std::vector<int> Fm_{};
  std::vector<int> Lm_{};
};

struct NuParent {
  NuParent() = default;

  NuParent(int Pdg, int DecMode, double DecP4[4], double DecX4[4], double ProP4[4], double ProX4[4], int ProNVtx)
    : Pdg_(Pdg), DecMode_(DecMode), DecP4_(4), DecX4_(4), ProP4_(4), ProX4_(4), ProNVtx_(ProNVtx) {
    for (int i = 0; i < 4; i++) {
      DecP4_[i] = DecP4[i];
      DecX4_[i] = DecX4[i];
      ProP4_[i] = ProP4[i];
      ProX4_[i] = ProX4[i];
    }
  }

  int Pdg_;
  int DecMode_;

  std::vector<double> DecP4_;
  std::vector<double> DecX4_;
  std::vector<double> ProP4_;
  std::vector<double> ProX4_;

  int ProNVtx_;
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
  std::vector<GRooTrackerEvent> events_;
  std::vector<StdHep> stdHeps_;
  std::optional<std::vector<NuParent>> nuParents_;
  std::optional<std::vector<NumiFlux>> numiFluxes_;
};
