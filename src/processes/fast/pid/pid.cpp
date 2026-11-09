#include "pid.hpp"

#include <caf/caf_wrapper.hpp>

#include <duneanaobj/StandardRecord/SRTrueParticle.h>
#include <duneanaobj/StandardRecord/SRTruthBranch.h>
#include <ufw/factory.hpp>

#include <cstddef>
#include <vector>

namespace sand::fast {

  namespace {

    /// True if `id` points to a true particle: primaries have no parent, so their parentID is left unset
    bool is_particle(::caf::TrueParticleID const& id) {
      return id.type == ::caf::TrueParticleID::kPrimary || id.type == ::caf::TrueParticleID::kSecondary;
    }

    /// The true particle `id` points to: truth.nu[id.ixn], then prim or sec at id.part
    ::caf::SRTrueParticle const& true_particle(::caf::SRTruthBranch const& truth, ::caf::TrueParticleID const& id) {
      auto const& true_ixn  = truth.nu.at(static_cast<std::size_t>(id.ixn));
      auto const& particles = (id.type == ::caf::TrueParticleID::kPrimary) ? true_ixn.prim : true_ixn.sec;
      return particles.at(static_cast<std::size_t>(id.part));
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

    /// The subdetector object a constituent names
    ::caf::SRRecoObjBase& object_at(::caf::SRSAND& sand, ::caf::SRSANDObjID const& id) {
      switch (id.type) {
      case ::caf::SRRecoBaseID::kSANDGRAINTrack:
        return sand.grain.tracks.at(id.idx);
      case ::caf::SRRecoBaseID::kSANDGRAINShower:
        return sand.grain.showers.at(id.idx);
      case ::caf::SRRecoBaseID::kSANDTrackerTrack:
        return sand.tracker.tracks.at(id.idx);
      case ::caf::SRRecoBaseID::kSANDTrackerShower:
        return sand.tracker.showers.at(id.idx);
      default:
        UFW_ERROR("Unsupported SAND collection {}", static_cast<int>(id.type));
      }
    }

    ::caf::RecoObjType reco_obj_type(::caf::SRRecoBaseID::SRRecoBaseCollectionType type) {
      switch (type) {
      case ::caf::SRRecoBaseID::kSANDGRAINTrack:
      case ::caf::SRRecoBaseID::kSANDTrackerTrack:
        return ::caf::RecoObjType::kTrack;
      case ::caf::SRRecoBaseID::kSANDGRAINShower:
      case ::caf::SRRecoBaseID::kSANDTrackerShower:
        return ::caf::RecoObjType::kShower;
      default:
        return ::caf::RecoObjType::kUnknownRecoObj;
      }
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

    // 1. One reco particle per assn, in the order of trkmatch: particle j is the particle of trkmatch[j]
    for (std::size_t i{}; i != common.ixn.sandreco.size(); ++i) {
      auto& ixn            = common.ixn.sandreco[i];
      auto const& trkmatch = sand.ixn[i].trkmatch;

      for (std::size_t j{}; j != trkmatch.size(); ++j) {
        auto const& id = trkmatch[j].trk.truth.at(0);
        ixn.part.sandreco.push_back(reco_particle_from_true(true_particle(truth, id), id));
      }
      ixn.part.nsandreco = static_cast<int>(ixn.part.sandreco.size());

      // Which true particles were seen: true particle -> its reco particle j, -1 if invisible
      auto const& true_ixn = truth.nu.at(ixn.truth.at(0));
      std::vector<int> prim_to_j(true_ixn.prim.size(), -1);
      std::vector<int> sec_to_j(true_ixn.sec.size(), -1);
      for (std::size_t j{}; j != trkmatch.size(); ++j) {
        auto const& id = trkmatch[j].trk.truth.at(0);
        (id.type == ::caf::TrueParticleID::kPrimary ? prim_to_j : sec_to_j).at(static_cast<std::size_t>(id.part)) =
            static_cast<int>(j);
      }
      auto const reco_index = [&](::caf::TrueParticleID const& id) {
        return (id.type == ::caf::TrueParticleID::kPrimary ? prim_to_j : sec_to_j)
            .at(static_cast<std::size_t>(id.part));
      };

      // The parent of a reco particle is its nearest visible true ancestor, -1 if none
      for (std::size_t j{}; j != trkmatch.size(); ++j) {
        // Climb the true ancestors until one was seen, or there are no more
        auto ancestor = true_particle(truth, trkmatch[j].trk.truth.at(0)).parentID;
        while (is_particle(ancestor) && reco_index(ancestor) < 0) {
          ancestor = true_particle(truth, ancestor).parentID;
        }
        if (is_particle(ancestor)) {
          auto const parent           = reco_index(ancestor);
          ixn.part.sandreco[j].parent = parent;
          ixn.part.sandreco[static_cast<std::size_t>(parent)].daughters.push_back(static_cast<unsigned int>(j));
        }
      }
    }

    // 2. Every assn and the particle at the same position point to each other; every constituent points to the particle
    for (std::size_t i{}; i != sand.ixn.size(); ++i) {
      auto& trkmatch = sand.ixn[i].trkmatch;
      for (std::size_t k{}; k != trkmatch.size(); ++k) {
        auto& assn = trkmatch[k];
        ::caf::SRRecoParticleID const part{static_cast<int>(i), ::caf::SRRecoParticleID::kSandreco,
                                           static_cast<int>(k)};

        assn.part = part;
        for (auto const& constituent : assn.constituents) {
          object_at(sand, constituent).part = part;
        }

        auto& particle           = common.ixn.sandreco.at(i).part.sandreco.at(k);
        particle.recoobj         = {static_cast<int>(i), ::caf::SRRecoBaseID::kSANDAssn, static_cast<int>(k)};
        particle.origRecoObjType = reco_obj_type(assn.constituents.front().type);
      }
    }
  }

} // namespace sand::fast

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::fast::pid);
