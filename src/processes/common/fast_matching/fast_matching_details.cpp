#include "fast_matching_details.hpp"

#include <duneanaobj/StandardRecord/SRDirectionBranch.h>
#include <duneanaobj/StandardRecord/SREnums.h>
#include <duneanaobj/StandardRecord/SRNeutrinoEnergyBranch.h>
#include <duneanaobj/StandardRecord/SRNeutrinoHypothesisBranch.h>
#include <duneanaobj/StandardRecord/SRRecoParticle.h>
#include <duneanaobj/StandardRecord/SRRecoParticlesBranch.h>
#include <duneanaobj/StandardRecord/SRTrueInteraction.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>

#include <duneanaobj/StandardRecord/SRVector3D.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace sand::common::reco_details {

  namespace {

    ::caf::SRVector3D normalize_to_direction(float px, float py, float pz) {
      float const mag = std::sqrt(px * px + py * py + pz * pz);
      return (mag > 0.f) ? ::caf::SRVector3D{px / mag, py / mag, pz / mag} : ::caf::SRVector3D{0.f, 0.f, 0.f};
    }

    /// One-hot encoding over the {0, 1, 2, N>=3} multiplicity buckets used by SRCVNScoreBranch.
    std::array<float, 4> count_bucket_one_hot(int count) {
      std::array bucket{0.f, 0.f, 0.f, 0.f};
      bucket[static_cast<std::size_t>(std::min(count, 3))] = 1.f;
      return bucket;
    }

    ::caf::SRTrueParticle const& true_particle_from_id(::caf::SRTrueInteraction const& true_ixn,
                                                       ::caf::TrueParticleID const& id) {
      return (id.type == ::caf::TrueParticleID::kPrimary) ? true_ixn.prim[id.part] : true_ixn.sec[id.part];
    }

    int reco_idx_from_id(::caf::TrueParticleID const& id, int n_prim) {
      switch (id.type) {
      case ::caf::TrueParticleID::kPrimary:
        return id.part;
      case ::caf::TrueParticleID::kSecondary:
        return n_prim + id.part;
      default:
        return -1; // no parent, or prefsi (not modeled here)
      }
    }
  } // namespace

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

  ::caf::SRDirectionBranch direction_from_true(::caf::SRTrueInteraction const& true_ixn) {
    ::caf::SRDirectionBranch dir{};

    auto const true_dir = normalize_to_direction(true_ixn.momentum.x, true_ixn.momentum.y, true_ixn.momentum.z);
    dir.calo            = true_dir;
    dir.heshw           = true_dir;
    dir.lngtrk          = true_dir;
    dir.part_mom_sum    = true_dir;

    return dir;
  }

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

  ::caf::SRRecoParticle reco_particle_from_true(::caf::SRTrueParticle const& true_part,
                                                ::caf::TrueParticleID const& id) {
    ::caf::SRRecoParticle reco_p{};

    reco_p.primary = (id.type == ::caf::TrueParticleID::kPrimary);
    reco_p.pdg     = true_part.pdg;
    reco_p.score   = 1.f;
    reco_p.E       = true_part.p.E;
    reco_p.p       = ::caf::SRVector3D{true_part.p.px, true_part.p.py, true_part.p.pz};
    reco_p.start   = true_part.start_pos;
    reco_p.end     = true_part.end_pos;

    reco_p.truth.push_back(id);
    reco_p.truthOverlap.push_back(1.f);

    return reco_p;
  }

  ::caf::SRRecoParticlesBranch reco_particles_from_true(::caf::SRTrueInteraction const& true_ixn, int ixn_idx) {
    ::caf::SRRecoParticlesBranch part{};
    auto const n_prim = static_cast<int>(true_ixn.prim.size());

    auto add_all = [&](std::vector<::caf::SRTrueParticle> const& particles, ::caf::TrueParticleID::PartType type) {
      for (std::size_t i{}; i != particles.size(); ++i) {
        ::caf::TrueParticleID const id{ixn_idx, type, static_cast<int>(i)};
        auto reco_p = reco_particle_from_true(particles[i], id);

        int const parent_idx = reco_idx_from_id(particles[i].parentID, n_prim);
        if (parent_idx >= 0) {
          reco_p.parent = parent_idx;
          part.sandreco[parent_idx].daughters.push_back(static_cast<unsigned int>(part.sandreco.size()));
        }

        part.sandreco.push_back(std::move(reco_p));
        ++part.nsandreco;
      }
    };

    add_all(true_ixn.prim, ::caf::TrueParticleID::kPrimary);
    add_all(true_ixn.sec, ::caf::TrueParticleID::kSecondary);
    return part;
  }

} // namespace sand::common::reco_details
