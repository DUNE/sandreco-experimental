// Check of the Kalman propagator against the true trajectories of a particle-gun edep-sim file.
//
// For each pair of consecutive true trajectory points of the primary particle inside the tracker, the true state at
// the first point is propagated to the z of the second point and compared with the true state there. The propagator
// has no material, so the residuals contain energy loss and multiple scattering between the two points: with the
// option "midp" the momentum of the start state is the average of the two points, which removes most of the energy
// loss and leaves the field and the equations.
//
// Not part of ctest: it needs a simulation file. Usage:
//   gun_propagation_check <edep-sim file> [B_x in T, default 0.6] [max events, default all]
//
// The magnetic field is uniform inside the region used, as in the GDML of nd_hall_with_lar_tms_sand_drift1
// (MagIntVol_volume, 0.6 T along global x).

#include <tracker/kalman/propagator.hpp>

#include <TFile.h>
#include <TG4Event.h>
#include <TTree.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace sand::tracker::kalman;

namespace {
  // Centre of volSAND in the global frame [mm], and the cylinder (axis along x) used to select tracker points
  double constexpr cy = -2384.73, cz = 23910.;
  double constexpr r_max = 1500., x_max = 1500.;

  bool in_tracker(const TLorentzVector& p) {
    return std::hypot(p.Y() - cy, p.Z() - cz) < r_max && std::abs(p.X()) < x_max;
  }

  struct stats {
    double n = 0, s = 0, s2 = 0;
    void add(double v) {
      n += 1;
      s += v;
      s2 += v * v;
    }
    double mean() const { return n ? s / n : 0; }
    double rms() const { return n ? std::sqrt(std::max(0., s2 / n - mean() * mean())) : 0; }
  };

  double mass_of(int pdg) {
    switch (std::abs(pdg)) {
    case 13:
      return 105.6583755;
    case 211:
      return 139.57039;
    case 2212:
      return 938.27208816;
    case 11:
      return 0.51099895;
    default:
      return 0.;
    }
  }
  int charge_of(int pdg) {
    switch (pdg) {
    case 13:
    case 11:
    case -211:
      return -1;
    case -13:
    case -11:
    case 211:
    case 2212:
      return +1;
    default:
      return 0;
    }
  }
} // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <edep-sim file> [Bx] [max events]\n", argv[0]);
    return 1;
  }
  double const bx   = argc > 2 ? std::atof(argv[2]) : 0.6;
  long const max_ev = argc > 3 ? std::atol(argv[3]) : -1;
  auto* f           = TFile::Open(argv[1]);
  auto* t           = f->Get<TTree>("EDepSimEvents");
  TG4Event* ev      = nullptr;
  t->SetBranchAddress("Event", &ev);
  uniform_field const bf({bx, 0., 0.});

  // index 0: momentum at the start point, 1: average momentum of the two points
  stats dx[2], dy[2], dt[2], dtx[2], dty[2], step;
  long const n = max_ev > 0 ? std::min<long>(max_ev, t->GetEntries()) : t->GetEntries();
  for (long i = 0; i < n; ++i) {
    t->GetEntry(i);
    for (auto const& tr : ev->Trajectories) {
      if (tr.ParentId != -1) {
        continue;
      }
      int const q    = charge_of(tr.PDGCode);
      double const m = mass_of(tr.PDGCode);
      if (q == 0) {
        continue;
      }
      for (size_t k = 0; k + 1 < tr.Points.size(); ++k) {
        auto const& a = tr.Points[k];
        auto const& b = tr.Points[k + 1];
        if (!in_tracker(a.Position) || !in_tracker(b.Position) || std::abs(a.Momentum.Z()) < 1e-3) {
          continue;
        }
        step.add(b.Position.Z() - a.Position.Z());
        for (int w = 0; w < 2; ++w) {
          double const p = w == 0 ? a.Momentum.Mag() : 0.5 * (a.Momentum.Mag() + b.Momentum.Mag());
          state s;
          s.z          = a.Position.Z();
          s.direction  = a.Momentum.Z() > 0 ? +1 : -1;
          s.par[X]     = a.Position.X();
          s.par[Y]     = a.Position.Y();
          s.par[TX]    = a.Momentum.X() / a.Momentum.Z();
          s.par[TY]    = a.Momentum.Y() / a.Momentum.Z();
          s.par[QOP]   = q / p;
          s.par[T]     = a.Position.T();
          auto const r = propagate(s, b.Position.Z(), m, bf);
          if (!r) {
            continue;
          }
          dx[w].add(r->out.par[X] - b.Position.X());
          dy[w].add(r->out.par[Y] - b.Position.Y());
          dt[w].add(r->out.par[T] - b.Position.T());
          dtx[w].add(r->out.par[TX] - b.Momentum.X() / b.Momentum.Z());
          dty[w].add(r->out.par[TY] - b.Momentum.Y() / b.Momentum.Z());
        }
      }
    }
  }
  std::printf("pairs of points: %.0f, mean step in z %.1f mm (rms %.1f)\n", step.n, step.mean(), step.rms());
  char const* label[2] = {"p at start point", "average p of the two points"};
  for (int w = 0; w < 2; ++w) {
    std::printf("%s\n", label[w]);
    std::printf("  dx  [mm] mean %+8.4f rms %7.4f\n", dx[w].mean(), dx[w].rms());
    std::printf("  dy  [mm] mean %+8.4f rms %7.4f   (y is the bending coordinate)\n", dy[w].mean(), dy[w].rms());
    std::printf("  dtx      mean %+9.5f rms %8.5f\n", dtx[w].mean(), dtx[w].rms());
    std::printf("  dty      mean %+9.5f rms %8.5f\n", dty[w].mean(), dty[w].rms());
    std::printf("  dt  [ns] mean %+8.4f rms %7.4f\n", dt[w].mean(), dt[w].rms());
  }
  return 0;
}
