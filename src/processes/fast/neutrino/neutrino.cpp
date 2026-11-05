#include "neutrino.hpp"

#include <caf/caf_wrapper.hpp>

#include <ufw/factory.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>

namespace sand::fast {

  namespace {

    /// One-hot encoding over the {0, 1, 2, N>=3} multiplicity buckets used by SRCVNScoreBranch.
    std::array<float, 4> count_bucket_one_hot(int count) {
      std::array bucket{0.f, 0.f, 0.f, 0.f};
      bucket[static_cast<std::size_t>(std::min(count, 3))] = 1.f;
      return bucket;
    }

    ::caf::SRVector3D normalize_to_direction(float px, float py, float pz) {
      float const mag = std::sqrt(px * px + py * py + pz * pz);
      return (mag > 0.f) ? ::caf::SRVector3D{px / mag, py / mag, pz / mag} : ::caf::SRVector3D{0.f, 0.f, 0.f};
    }

    /// Perfect classifier: one-hot CVN scores on the true interaction class.
    ::caf::SRNeutrinoHypothesisBranch neutrino_hypothesis_from_true(::caf::SRTrueInteraction const& true_ixn) {
      ::caf::SRNeutrinoHypothesisBranch nuhyp{};
      auto& cvn = nuhyp.cvn;

      int const abs_pdg = std::abs(true_ixn.pdg);
      cvn.isnubar       = (true_ixn.pdg < 0) ? 1.f : 0.f;
      cvn.nc            = true_ixn.iscc ? 0.f : 1.f;
      cvn.nue           = (true_ixn.iscc && abs_pdg == 12) ? 1.f : 0.f;
      cvn.numu          = (true_ixn.iscc && abs_pdg == 14) ? 1.f : 0.f;
      cvn.nutau         = (true_ixn.iscc && abs_pdg == 16) ? 1.f : 0.f;

      auto const [p0, p1, p2, pN] = count_bucket_one_hot(true_ixn.nproton);
      cvn.protons0                = p0;
      cvn.protons1                = p1;
      cvn.protons2                = p2;
      cvn.protonsN                = pN;

      auto const [c0, c1, c2, cN] = count_bucket_one_hot(true_ixn.npip + true_ixn.npim);
      cvn.chgpi0                  = c0;
      cvn.chgpi1                  = c1;
      cvn.chgpi2                  = c2;
      cvn.chgpiN                  = cN;

      auto const [z0, z1, z2, zN] = count_bucket_one_hot(true_ixn.npi0);
      cvn.pizero0                 = z0;
      cvn.pizero1                 = z1;
      cvn.pizero2                 = z2;
      cvn.pizeroN                 = zN;

      auto const [n0, n1, n2, nN] = count_bucket_one_hot(true_ixn.nneutron);
      cvn.neutron0                = n0;
      cvn.neutron1                = n1;
      cvn.neutron2                = n2;
      cvn.neutronN                = nN;

      return nuhyp;
    }

    /// Perfect neutrino energy hypothesis: every estimator collapses to the true energy.
    ::caf::SRNeutrinoEnergyBranch energy_from_true(::caf::SRTrueInteraction const& true_ixn) {
      ::caf::SRNeutrinoEnergyBranch enu{};

      enu.calo        = true_ixn.E;
      enu.lep_calo    = true_ixn.E;
      enu.mu_range    = true_ixn.E;
      enu.mu_mcs      = true_ixn.E;
      enu.mu_mcs_llhd = true_ixn.E;
      enu.e_calo      = true_ixn.E;

      enu.e_had  = true_ixn.E;
      enu.mu_had = true_ixn.E;

      enu.regcnn = true_ixn.E;

      enu.part_energy_sum = true_ixn.E;

      return enu;
    }

    /// Perfect direction hypothesis: every estimator collapses to the true neutrino direction.
    ::caf::SRDirectionBranch direction_from_true(::caf::SRTrueInteraction const& true_ixn) {
      ::caf::SRDirectionBranch dir{};

      auto const true_dir = normalize_to_direction(true_ixn.momentum.x, true_ixn.momentum.y, true_ixn.momentum.z);
      dir.calo            = true_dir;
      dir.heshw           = true_dir;
      dir.lngtrk          = true_dir;
      dir.part_mom_sum    = true_dir;

      return dir;
    }

  } // namespace

  neutrino::neutrino()
    : process{{{"in_truth", "sand::caf::truth_branch_wrapper"}, {"in_common", "sand::caf::common_reco_branch_wrapper"}},
              {{"out_common", "sand::caf::common_reco_branch_wrapper"}}} {}

  void neutrino::run() {
    auto const& truth                 = get<sand::caf::truth_branch_wrapper>("in_truth");
    ::caf::SRCommonRecoBranch& common = set<sand::caf::common_reco_branch_wrapper>("out_common");

    // The interactions and particles of the pid, enriched below
    common = get<sand::caf::common_reco_branch_wrapper>("in_common");

    // The hypotheses on the neutrino of every interaction, from the true interaction it comes from
    for (auto& ixn : common.ixn.sandreco) {
      auto const& true_ixn = truth.nu.at(ixn.truth.at(0));
      ixn.nuhyp            = neutrino_hypothesis_from_true(true_ixn);
      ixn.Enu              = energy_from_true(true_ixn);
      ixn.dir              = direction_from_true(true_ixn);
    }
  }

} // namespace sand::fast

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::fast::neutrino);
