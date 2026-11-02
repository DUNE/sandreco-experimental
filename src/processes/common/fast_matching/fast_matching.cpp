#include "fast_matching.hpp"
#include "fast_matching_details.hpp"

#include <caf/caf_wrapper.hpp>

#include <ufw/factory.hpp>

namespace sand::common {

  namespace {

    void fill_interactions(::caf::SRTruthBranch const& truth, ::caf::SRCommonRecoBranch& common, ::caf::SRSAND& sand) {
      for (std::size_t ixn_idx{}; ixn_idx != truth.nu.size(); ++ixn_idx) {
        auto const& true_ixn = truth.nu[ixn_idx];

        auto& ixn        = common.ixn.sandreco.emplace_back();
        ixn.id           = true_ixn.id;
        ixn.vtx          = true_ixn.vtx;
        ixn.dir          = reco_details::direction_from_true(true_ixn);
        ixn.nuhyp        = reco_details::neutrino_hypothesis_from_true(true_ixn);
        ixn.Enu          = reco_details::energy_from_true(true_ixn);
        ixn.part         = reco_details::reco_particles_from_true(true_ixn, ixn_idx);
        ixn.truth        = {ixn_idx};
        ixn.truthOverlap = {1.f};
      }
      common.ixn.nsandreco = common.ixn.sandreco.size();

      // Index-aligned with common.ixn.sandreco: SRRecoBaseID{ixn, kSANDAssn, i} resolves to sand.ixn[ixn].trkmatch[i]
      sand.ixn.resize(truth.nu.size());
      sand.nixn = sand.ixn.size();
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

    void associate(reco_details::ParticleMatch const& match, ::caf::SRTruthBranch const& truth,
                   ::caf::SRCommonRecoBranch& common, ::caf::SRSAND& sand) {
      auto const& id    = match.particle;
      auto const n_prim = static_cast<int>(truth.nu.at(id.ixn).prim.size());
      int const ipart   = id.type == ::caf::TrueParticleID::kPrimary ? id.part : n_prim + id.part;
      ::caf::SRRecoParticleID const part{id.ixn, ::caf::SRRecoParticleID::kSandreco, ipart};

      auto& sand_ixn     = sand.ixn.at(id.ixn);
      auto& assn         = sand_ixn.trkmatch.emplace_back();
      assn.constituents  = match.constituents;
      assn.part          = part;
      sand_ixn.ntrkmatch = sand_ixn.trkmatch.size();

      for (auto const& constituent : match.constituents) {
        object_at(sand, constituent).part = part;
      }

      auto& particle   = common.ixn.sandreco.at(id.ixn).part.sandreco.at(ipart);
      particle.recoobj = {id.ixn, ::caf::SRRecoBaseID::kSANDAssn, static_cast<int>(sand_ixn.trkmatch.size() - 1)};
      particle.origRecoObjType = reco_obj_type(match.constituents.front().type);
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

    fill_interactions(truth, common, sand);
    sand.tracker = get<sand::caf::tracker_reco_wrapper>("in_tracker");

    for (auto const& match : reco_details::match_by_truth(sand)) {
      associate(match, truth, common, sand);
    }
  }

} // namespace sand::common

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::common::fast_matching);
