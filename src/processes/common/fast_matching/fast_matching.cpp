#include "fast_matching.hpp"
#include "fast_kinematics.hpp"
#include "fast_pid.hpp"
#include "fast_vertexing.hpp"
#include "truth_matching.hpp"

#include <caf/caf_wrapper.hpp>

#include <ufw/factory.hpp>

#include <cstddef>
#include <utility>

namespace sand::common {

  namespace {

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

    /// Links every SRSANDAssn both ways to the reco particle of its trk.truth[0], and every constituent to it
    void link_particles_to_assns(::caf::SRTruthBranch const& truth, ::caf::SRCommonRecoBranch& common,
                                 ::caf::SRSAND& sand) {
      for (std::size_t i{}; i != sand.ixn.size(); ++i) {
        auto& trkmatch = sand.ixn[i].trkmatch;
        for (std::size_t k{}; k != trkmatch.size(); ++k) {
          auto& assn = trkmatch[k];

          // Position of the true particle in SRRecoParticlesBranch::sandreco: primaries first, then secondaries
          auto const& id    = assn.trk.truth.at(0);
          auto const n_prim = static_cast<int>(truth.nu.at(id.ixn).prim.size());
          int const ipart   = id.type == ::caf::TrueParticleID::kPrimary ? id.part : n_prim + id.part;
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

  } // namespace

  fast_matching::fast_matching()
    : process{
          {{"in_truth", "sand::caf::truth_branch_wrapper"}, {"in_tracker", "sand::caf::tracker_reco_wrapper"}},
          {{"out_common", "sand::caf::common_reco_branch_wrapper"}, {"out_nd", "sand::caf::nd_reco_branch_wrapper"}}} {}

  void fast_matching::run() {
    auto const& truth = get<sand::caf::truth_branch_wrapper>("in_truth");
    auto& common      = set<sand::caf::common_reco_branch_wrapper>("out_common");
    auto& sand        = set<sand::caf::nd_reco_branch_wrapper>("out_nd").sand;

    sand.tracker = get<sand::caf::tracker_reco_wrapper>("in_tracker");

    auto vertices        = vertex_by_truth(truth, match_by_truth(sand));
    sand.ixn             = std::move(vertices.sand);
    sand.nixn            = sand.ixn.size();
    common.ixn.sandreco  = std::move(vertices.interactions);
    common.ixn.nsandreco = common.ixn.sandreco.size();

    for (std::size_t i{}; i != truth.nu.size(); ++i) {
      auto const& true_ixn = truth.nu[i];
      auto& ixn            = common.ixn.sandreco[i];
      ixn.part             = reco_particles_from_true(true_ixn, static_cast<int>(i));
      ixn.nuhyp            = neutrino_hypothesis_from_true(true_ixn);
      ixn.Enu              = energy_from_true(true_ixn);
      ixn.dir              = direction_from_true(true_ixn);
    }

    link_particles_to_assns(truth, common, sand);
  }

} // namespace sand::common

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::common::fast_matching);
