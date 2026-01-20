#pragma once

#include <Math/Vector3D.h>
#include <Math/Vector4D.h>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace genie {
  typedef enum EGHepStatus {
    kIStUndefined                = -1,
    kIStInitialState             = 0, /* generator-level initial state */
    kIStStableFinalState         = 1, /* generator-level final state: particles to be tracked by detector-level MC */
    kIStIntermediateState        = 2,
    kIStDecayedState             = 3,
    kIStCorrelatedNucleon        = 10,
    kIStNucleonTarget            = 11,
    kIStDISPreFragmHadronicState = 12,
    kIStPreDecayResonantState    = 13,
    kIStHadronInTheNucleus       = 14, /* hadrons inside the nucleus: marked for hadron transport modules to act on */
    kIStFinalStateNuclearRemnant =
        15, /* low energy nuclear fragments entering the record collectively as a 'hadronic blob' pseudo-particle */
    kIStNucleonClusterTarget = 16 // for composite nucleons before phase space decay
  } GHepStatus_t;
} // namespace genie

struct GRooTrackerEvent {
  GRooTrackerEvent() = default;

  int EvtNum_;
  double EvtXSec_;
  double EvtDXSec_;
  double EvtKPS_;
  double EvtWght_;
  double EvtProb_;
  std::array<double, 4> EvtVtx_;
  std::string EvtCode_;
};

enum class StdHepIndex : int { nu = 0, tgt = 1 };

struct StdHep {
  StdHep() = default;

  StdHep(int N, const int Pdg[], const int Status[], const int Rescat[], const double X4[][4], const double P4[][4],
         const double Polz[][3], const int Fd[], const int Ld[], const int Fm[], const int Lm[]);

  [[nodiscard]] std::vector<int> daughters_indexes_of_part(int particle_index) const;

  int N_{};
  std::vector<int> Pdg_{};
  std::vector<genie::GHepStatus_t> Status_{};
  std::vector<int> Rescat_{};

  std::vector<ROOT::Math::XYZTVector> X4_{}; //[fm]
  std::vector<ROOT::Math::PxPyPzEVector> P4_{};
  std::vector<ROOT::Math::XYZVector> Polz_{};

  std::vector<int> Fd_{};
  std::vector<int> Ld_{};
  std::vector<int> Fm_{};
  std::vector<int> Lm_{};
};

struct NuParent {
  NuParent() = default;

  NuParent(const int Pdg, const int DecMode, const double DecP4[4], const double DecX4[4], const double ProP4[4],
           const double ProX4[4], const int ProNVtx)
    : Pdg_{Pdg},
      DecMode_{DecMode},
      DecP4_{DecP4[0], DecP4[1], DecP4[2], DecP4[3]},
      DecX4_{DecX4[0], DecX4[1], DecX4[2], DecX4[3]},
      ProP4_{ProP4[0], ProP4[1], ProP4[2], ProP4[3]},
      ProX4_{ProX4[0], ProX4[1], ProX4[2], ProX4[3]},
      ProNVtx_{ProNVtx} {}

  int Pdg_;
  int DecMode_;

  ROOT::Math::PxPyPzEVector DecP4_;
  ROOT::Math::XYZTVector DecX4_;
  ROOT::Math::PxPyPzEVector ProP4_;
  ROOT::Math::XYZTVector ProX4_;

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
