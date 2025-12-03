#include "GenieWrapper.h"

StdHep::StdHep(const int N, const int Pdg[], const int Status[], const int Rescat[], const double X4[][4],
               const double P4[][4], const double Polz[][3], const int Fd[], const int Ld[], const int Fm[],
               const int Lm[])
  : N_{N} {
  Pdg_.reserve(N);
  Status_.reserve(N);
  Rescat_.reserve(N);
  X4_.reserve(N);
  P4_.reserve(N);
  Polz_.reserve(N);
  Fd_.reserve(N);
  Ld_.reserve(N);
  Fm_.reserve(N);
  Lm_.reserve(N);
  for (int i = 0; i < N; i++) {
    Pdg_.push_back(Pdg[i]);
    Status_.push_back(static_cast<genie::GHepStatus_t>(Status[i]));
    Rescat_.push_back(Rescat[i]);

    X4_.emplace_back(X4[i][0], X4[i][1], X4[i][2], X4[i][3]);
    P4_.emplace_back(P4[i][0], P4[i][1], P4[i][2], P4[i][3]);

    Polz_.emplace_back(Polz[i][0], Polz[i][1], Polz[i][2]);

    Fd_.push_back(Fd[i]);
    Ld_.push_back(Ld[i]);
    Fm_.push_back(Fm[i]);
    Lm_.push_back(Lm[i]);
  }
}

std::vector<int> StdHep::daughters_indexes_of_part(const int particle_index) const {
  if (Fd_[particle_index] == -1) {
    return {};
  }

  std::vector<int> output;
  for (int i = Fd_[particle_index]; i < Ld_[particle_index] + 1; i++) {
    output.push_back(i);
  }
  return output;
}
