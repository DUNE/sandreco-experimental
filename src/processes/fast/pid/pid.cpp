#include "pid.hpp"

#include <caf/caf_wrapper.hpp>

#include <ufw/factory.hpp>

#include <cstddef>
#include <utility>
#include <vector>

namespace sand::fast {

  namespace {

    /// Position of a true particle in SRRecoParticlesBranch::sandreco: primaries first, then secondaries
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

    /// Perfect reco particle: every field copied or derived 1:1 from the true particle
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

    /// Perfect reco particles: one per true `prim`/`sec`, in that order, with parent/daughters resolved
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

    /// The subdetector object a constituent names
    ::caf::SRRecoObjBase& object_at(::caf::SRSAND& sand, ::caf::SRSANDObjID const& id) {
      switch (id.type) {
      case ::caf::SRRecoBaseID::kSANDTrackerTrack:
        return sand.tracker.tracks.at(id.idx);
      case ::caf::SRRecoBaseID::kSANDTrackerShower:
        return sand.tracker.showers.at(id.idx);
      default:
        UFW_ERROR("Unsupported SAND collection {}", static_cast<int>(id.type));
      }
    }

    ::caf::RecoObjType reco_obj_type(::caf::SRRecoBaseID::SRRecoBaseCollectionType type) {
      return type == ::caf::SRRecoBaseID::kSANDTrackerShower ? ::caf::RecoObjType::kShower : ::caf::RecoObjType::kTrack;
    }

  } // namespace

  pid::pid()
    : process{
          {{"in_truth", "sand::caf::truth_branch_wrapper"},
           {"in_common", "sand::caf::common_reco_branch_wrapper"},
           {"in_nd", "sand::caf::nd_reco_branch_wrapper"}},
          {{"out_common", "sand::caf::common_reco_branch_wrapper"}, {"out_nd", "sand::caf::nd_reco_branch_wrapper"}}} {}

  void pid::run() {
    auto const& truth                 = get<sand::caf::truth_branch_wrapper>("in_truth");
    ::caf::SRCommonRecoBranch& common = set<sand::caf::common_reco_branch_wrapper>("out_common");
    ::caf::SRNDBranch& nd             = set<sand::caf::nd_reco_branch_wrapper>("out_nd");

    // The interactions and the assns of the vertexing, enriched below.
    common     = get<sand::caf::common_reco_branch_wrapper>("in_common");
    nd         = get<sand::caf::nd_reco_branch_wrapper>("in_nd");
    auto& sand = nd.sand;

    // 1. One reco particle per true particle of the interaction, in prim/sec order
    for (auto& ixn : common.ixn.sandreco) {
      auto const true_idx = ixn.truth.at(0);
      ixn.part            = reco_particles_from_true(truth.nu.at(true_idx), static_cast<int>(true_idx));
    }

    // 2. Every assn and the particle of its trk.truth[0] point to each other; every constituent points to the particle
    for (std::size_t i{}; i != sand.ixn.size(); ++i) {
      auto& trkmatch = sand.ixn[i].trkmatch;
      for (std::size_t k{}; k != trkmatch.size(); ++k) {
        auto& assn        = trkmatch[k];
        auto const& id    = assn.trk.truth.at(0);
        auto const n_prim = static_cast<int>(truth.nu.at(id.ixn).prim.size());
        int const ipart   = reco_idx_from_id(id, n_prim);
        ::caf::SRRecoParticleID const part{static_cast<int>(i), ::caf::SRRecoParticleID::kSandreco, ipart};

        assn.part = part;
        for (auto const& constituent : assn.constituents) {
          object_at(sand, constituent).part = part;
        }

        auto& particle           = common.ixn.sandreco.at(i).part.sandreco.at(ipart);
        particle.recoobj         = {static_cast<int>(i), ::caf::SRRecoBaseID::kSANDAssn, static_cast<int>(k)};
        particle.origRecoObjType = reco_obj_type(assn.constituents.front().type);
      }
    }
  }

} // namespace sand::fast

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::fast::pid);
