#include "fast_kinematics.hpp"

#include <duneanaobj/StandardRecord/SRDirectionBranch.h>
#include <duneanaobj/StandardRecord/SRNeutrinoEnergyBranch.h>
#include <duneanaobj/StandardRecord/SRTrueInteraction.h>
#include <duneanaobj/StandardRecord/SRVector3D.h>

#include <cmath>

namespace sand::common {

  namespace {

    ::caf::SRVector3D normalize_to_direction(float px, float py, float pz) {
      float const mag = std::sqrt(px * px + py * py + pz * pz);
      return (mag > 0.f) ? ::caf::SRVector3D{px / mag, py / mag, pz / mag} : ::caf::SRVector3D{0.f, 0.f, 0.f};
    }

  } // namespace

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

  ::caf::SRDirectionBranch direction_from_true(::caf::SRTrueInteraction const& true_ixn) {
    ::caf::SRDirectionBranch dir{};

    auto const true_dir = normalize_to_direction(true_ixn.momentum.x, true_ixn.momentum.y, true_ixn.momentum.z);
    dir.calo            = true_dir;
    dir.heshw           = true_dir;
    dir.lngtrk          = true_dir;
    dir.part_mom_sum    = true_dir;

    return dir;
  }

} // namespace sand::common
