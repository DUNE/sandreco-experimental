#include <caf/caf_wrapper.hpp>

#include <edep_reader/edep_reader.hpp>
#include <spill_maps/spill_maps.hpp>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace sand::test {

  /// Verifies that truth_filler's build_true_particle_tree() produces a consistent,
  /// round-trip navigable event tree: every TrueParticleID link (ancestor_id, parentID,
  /// daughtersID) resolves to the node it claims to, in both directions.
  class truth_filler_test : public ufw::process {
    std::size_t m_multi_ixn_spills{}; ///< spills with more than one interaction, where local and edep-sim ids differ
   public:
    truth_filler_test();
    ~truth_filler_test() override;
    void run() override;
  };

  truth_filler_test::truth_filler_test() : process({{"in_truth", "sand::caf::truth_branch_wrapper"}}, {}) {}

  namespace {
    bool same_id(::caf::TrueParticleID const& a, ::caf::TrueParticleID const& b) {
      return a.ixn == b.ixn && a.type == b.type && a.part == b.part;
    }

    bool contains(std::vector<::caf::TrueParticleID> const& ids, ::caf::TrueParticleID const& target) {
      return std::any_of(ids.begin(), ids.end(), [&](auto const& id) { return same_id(id, target); });
    }

    ::caf::SRTrueParticle const& resolve(::caf::SRTrueInteraction const& ixn, ::caf::TrueParticleID const& id) {
      UFW_ASSERT(id.type == ::caf::TrueParticleID::kPrimary || id.type == ::caf::TrueParticleID::kSecondary,
                 "Unexpected TrueParticleID::type {}", static_cast<int>(id.type));
      auto const& collection = (id.type == ::caf::TrueParticleID::kPrimary) ? ixn.prim : ixn.sec;
      UFW_ASSERT(id.part >= 0 && static_cast<std::size_t>(id.part) < collection.size(),
                 "TrueParticleID::part {} out of range (collection size {})", id.part, collection.size());
      return collection[static_cast<std::size_t>(id.part)];
    }
  } // namespace

  void truth_filler_test::run() {
    auto const& truth = get<sand::caf::truth_branch_wrapper>("in_truth");
    auto const& edep  = instance<sand::edep_reader>();

    UFW_ASSERT(truth.nu.size() == truth.nnu, "nnu ({}) doesn't match nu.size() ({})", truth.nnu, truth.nu.size());

    if (truth.nu.size() > 1) {
      ++m_multi_ixn_spills;
    }

    // SRTrueParticle::G4ID (per interaction) -> spill-wide edep-sim track id
    auto const& track_ids  = instance<sand::spill_maps>();
    auto const to_spill_id = [&](int ixn_idx, int g4id) {
      return track_ids.at(static_cast<std::size_t>(ixn_idx)).to_spill_id(g4id);
    };

    // The trajectory a G4ID points to must be the one the particle was built from: same pdg, start and parent
    auto const check_trajectory = [&](::caf::SRTrueParticle const& p, std::size_t ixn_idx, char const* coll,
                                      std::size_t k) {
      auto const ixn_i = static_cast<int>(ixn_idx);
      auto const trj   = edep.GetTrajectory(to_spill_id(ixn_i, p.G4ID));
      UFW_ASSERT(trj != edep.end(), "nu[{}].{}[{}]: G4ID {} has no edep-sim trajectory", ixn_idx, coll, k, p.G4ID);
      UFW_ASSERT(trj->GetPDGCode() == p.pdg, "nu[{}].{}[{}]: G4ID {} resolves to pdg {}, but the particle is pdg {}",
                 ixn_idx, coll, k, p.G4ID, trj->GetPDGCode(), p.pdg);
      UFW_ASSERT(to_spill_id(ixn_i, p.parent) == trj->GetParentId(),
                 "nu[{}].{}[{}]: parent {} resolves to edep-sim track {}, but the trajectory's parent is {}", ixn_idx,
                 coll, k, p.parent, to_spill_id(ixn_i, p.parent), trj->GetParentId());
      if (auto const points = trj->GetTrajectoryPointsVect(); !points.empty()) {
        auto const& start = points.front().GetPosition();
        UFW_ASSERT(std::abs(start.X() * 0.1 - p.start_pos.x) < 1e-3 && std::abs(start.Y() * 0.1 - p.start_pos.y) < 1e-3
                       && std::abs(start.Z() * 0.1 - p.start_pos.z) < 1e-3,
                   "nu[{}].{}[{}]: G4ID {} resolves to a trajectory starting at ({}, {}, {}) cm, but the particle "
                   "starts at ({}, {}, {}) cm",
                   ixn_idx, coll, k, p.G4ID, start.X() * 0.1, start.Y() * 0.1, start.Z() * 0.1, p.start_pos.x,
                   p.start_pos.y, p.start_pos.z);
      }
    };

    for (std::size_t ixn_idx{}; ixn_idx != truth.nu.size(); ++ixn_idx) {
      auto const& ixn   = truth.nu[ixn_idx];
      auto const sr_ixn = static_cast<int>(ixn_idx);

      UFW_ASSERT(static_cast<std::size_t>(ixn.nprim) == ixn.prim.size(), "nu[{}]: nprim doesn't match prim.size()",
                 ixn_idx);
      UFW_ASSERT(static_cast<std::size_t>(ixn.nsec) == ixn.sec.size(), "nu[{}]: nsec doesn't match sec.size()",
                 ixn_idx);

      int nproton{}, nneutron{}, npip{}, npim{}, npi0{};

      for (std::size_t k{}; k != ixn.prim.size(); ++k) {
        auto const& p = ixn.prim[k];
        ::caf::TrueParticleID const self_id{sr_ixn, ::caf::TrueParticleID::kPrimary, static_cast<int>(k)};

        check_trajectory(p, ixn_idx, "prim", k);

        UFW_ASSERT(same_id(p.ancestor_id, self_id), "nu[{}].prim[{}]: ancestor_id doesn't point to itself", ixn_idx, k);
        UFW_ASSERT(p.parentID.type == ::caf::TrueParticleID::kUnknown,
                   "nu[{}].prim[{}]: parentID should be unset (no SR parent for a primary)", ixn_idx, k);
        UFW_ASSERT(p.daughters.size() == p.daughtersID.size(), "nu[{}].prim[{}]: daughters/daughtersID size mismatch",
                   ixn_idx, k);

        for (auto const& did : p.daughtersID) {
          UFW_ASSERT(did.ixn == sr_ixn && did.type == ::caf::TrueParticleID::kSecondary,
                     "nu[{}].prim[{}]: daughter isn't a kSecondary of this interaction", ixn_idx, k);
          auto const& child = resolve(ixn, did);
          UFW_ASSERT(same_id(child.parentID, self_id), "nu[{}].prim[{}]: daughter's parentID doesn't point back",
                     ixn_idx, k);
          UFW_ASSERT(same_id(child.ancestor_id, self_id), "nu[{}].prim[{}]: daughter's ancestor_id isn't this primary",
                     ixn_idx, k);
        }

        switch (p.pdg) {
        case 2212:
          ++nproton;
          break;
        case 2112:
          ++nneutron;
          break;
        case 211:
          ++npip;
          break;
        case -211:
          ++npim;
          break;
        case 111:
          ++npi0;
          break;
        default:
          break;
        }
      }

      UFW_ASSERT(nproton == ixn.nproton, "nu[{}]: nproton {} != counted {}", ixn_idx, ixn.nproton, nproton);
      UFW_ASSERT(nneutron == ixn.nneutron, "nu[{}]: nneutron {} != counted {}", ixn_idx, ixn.nneutron, nneutron);
      UFW_ASSERT(npip == ixn.npip, "nu[{}]: npip {} != counted {}", ixn_idx, ixn.npip, npip);
      UFW_ASSERT(npim == ixn.npim, "nu[{}]: npim {} != counted {}", ixn_idx, ixn.npim, npim);
      UFW_ASSERT(npi0 == ixn.npi0, "nu[{}]: npi0 {} != counted {}", ixn_idx, ixn.npi0, npi0);

      for (std::size_t k{}; k != ixn.sec.size(); ++k) {
        auto const& s = ixn.sec[k];
        ::caf::TrueParticleID const self_id{sr_ixn, ::caf::TrueParticleID::kSecondary, static_cast<int>(k)};

        check_trajectory(s, ixn_idx, "sec", k);

        UFW_ASSERT(s.ancestor_id.ixn == sr_ixn && s.ancestor_id.type == ::caf::TrueParticleID::kPrimary,
                   "nu[{}].sec[{}]: ancestor_id isn't a kPrimary of this interaction", ixn_idx, k);
        UFW_ASSERT(s.ancestor_id.part >= 0 && static_cast<std::size_t>(s.ancestor_id.part) < ixn.prim.size(),
                   "nu[{}].sec[{}]: ancestor_id.part out of range", ixn_idx, k);

        UFW_ASSERT(s.parentID.ixn == sr_ixn, "nu[{}].sec[{}]: parentID.ixn doesn't match this interaction", ixn_idx, k);
        UFW_ASSERT(s.parentID.type == ::caf::TrueParticleID::kPrimary
                       || s.parentID.type == ::caf::TrueParticleID::kSecondary,
                   "nu[{}].sec[{}]: parentID has unexpected type {}", ixn_idx, k, static_cast<int>(s.parentID.type));
        if (s.parentID.type == ::caf::TrueParticleID::kSecondary) {
          // Nodes are appended in pre-order, so a secondary parent must already exist.
          UFW_ASSERT(s.parentID.part < static_cast<int>(k),
                     "nu[{}].sec[{}]: parent secondary wasn't inserted before its child (pre-order broken)", ixn_idx,
                     k);
        }

        auto const& parent = resolve(ixn, s.parentID);
        UFW_ASSERT(contains(parent.daughtersID, self_id), "nu[{}].sec[{}]: parent's daughtersID doesn't list it back",
                   ixn_idx, k);

        UFW_ASSERT(s.daughters.size() == s.daughtersID.size(), "nu[{}].sec[{}]: daughters/daughtersID size mismatch",
                   ixn_idx, k);
      }
    }

    UFW_INFO("truth_filler_test: verified {} interaction(s)", truth.nu.size());
  }

  truth_filler_test::~truth_filler_test() {
    UFW_INFO("truth_filler_test: {} spill(s) with more than one interaction", m_multi_ixn_spills);
    UFW_ASSERT(m_multi_ixn_spills > 0, "no spill with more than one interaction over the whole job: local and "
                                       "edep-sim track ids coincide, so the G4ID checks verified nothing");
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::truth_filler_test)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::truth_filler_test)
