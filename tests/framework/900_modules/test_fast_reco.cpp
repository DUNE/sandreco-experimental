#include <caf/caf_wrapper.hpp>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <algorithm>
#include <cstddef>

namespace sand::test {

  /// Verifies that sand::common::fast_reco's output is internally consistent and
  /// round-trip navigable: every SRRecoParticle <-> SRTrack/SRShower link (recoobj/part)
  /// resolves back to the node it claims to, every truth match resolves to a real
  /// SRTrueParticle with the same pdg, and every parent/daughters link round-trips.
  struct fast_reco_test : public ufw::process {
    fast_reco_test();
    void run() override;
  };

  fast_reco_test::fast_reco_test()
    : process({{"in_truth", "sand::caf::truth_branch_wrapper"},
               {"in_common", "sand::caf::common_reco_branch_wrapper"},
               {"in_nd", "sand::caf::nd_reco_branch_wrapper"}},
              {}) {}

  namespace {
    ::caf::SRTrueParticle const& resolve_true(::caf::SRTruthBranch const& truth, ::caf::TrueParticleID const& id) {
      UFW_ASSERT(id.ixn >= 0 && static_cast<std::size_t>(id.ixn) < truth.nu.size(),
                 "TrueParticleID::ixn {} out of range", id.ixn);
      auto const& ixn = truth.nu[static_cast<std::size_t>(id.ixn)];
      UFW_ASSERT(id.type == ::caf::TrueParticleID::kPrimary || id.type == ::caf::TrueParticleID::kSecondary,
                 "Unexpected TrueParticleID::type {}", static_cast<int>(id.type));
      auto const& collection = (id.type == ::caf::TrueParticleID::kPrimary) ? ixn.prim : ixn.sec;
      UFW_ASSERT(id.part >= 0 && static_cast<std::size_t>(id.part) < collection.size(),
                 "TrueParticleID::part {} out of range", id.part);
      return collection[static_cast<std::size_t>(id.part)];
    }
  } // namespace

  void fast_reco_test::run() {
    auto const& truth  = get<sand::caf::truth_branch_wrapper>("in_truth");
    auto const& common = get<sand::caf::common_reco_branch_wrapper>("in_common");
    auto const& nd     = get<sand::caf::nd_reco_branch_wrapper>("in_nd");

    UFW_ASSERT(common.ixn.sandreco.size() == common.ixn.nsandreco,
               "common.ixn.nsandreco ({}) doesn't match common.ixn.sandreco.size() ({})", common.ixn.nsandreco,
               common.ixn.sandreco.size());
    UFW_ASSERT(nd.sand.ixn.size() == nd.sand.nixn, "nd.sand.nixn ({}) doesn't match nd.sand.ixn.size() ({})",
               nd.sand.nixn, nd.sand.ixn.size());
    UFW_ASSERT(common.ixn.sandreco.size() == nd.sand.ixn.size(),
               "common.ixn.sandreco.size() ({}) doesn't match nd.sand.ixn.size() ({})", common.ixn.sandreco.size(),
               nd.sand.ixn.size());
    UFW_ASSERT(common.ixn.sandreco.size() == truth.nu.size(),
               "common.ixn.sandreco.size() ({}) doesn't match truth.nu.size() ({})", common.ixn.sandreco.size(),
               truth.nu.size());

    for (std::size_t ixn_idx{}; ixn_idx != common.ixn.sandreco.size(); ++ixn_idx) {
      auto const& reco_ixn = common.ixn.sandreco[ixn_idx];
      auto const& sand_ixn = nd.sand.ixn[ixn_idx];
      auto const ixn_idx_i = static_cast<int>(ixn_idx);

      UFW_ASSERT(reco_ixn.part.sandreco.size() == static_cast<std::size_t>(reco_ixn.part.nsandreco),
                 "nu[{}]: part.nsandreco ({}) doesn't match part.sandreco.size() ({})", ixn_idx,
                 reco_ixn.part.nsandreco, reco_ixn.part.sandreco.size());
      UFW_ASSERT(sand_ixn.tracker.tracks.size() == sand_ixn.tracker.ntracks,
                 "nu[{}]: tracker.ntracks ({}) doesn't match tracker.tracks.size() ({})", ixn_idx,
                 sand_ixn.tracker.ntracks, sand_ixn.tracker.tracks.size());
      UFW_ASSERT(sand_ixn.tracker.showers.size() == sand_ixn.tracker.nshowers,
                 "nu[{}]: tracker.nshowers ({}) doesn't match tracker.showers.size() ({})", ixn_idx,
                 sand_ixn.tracker.nshowers, sand_ixn.tracker.showers.size());

      for (std::size_t p{}; p != reco_ixn.part.sandreco.size(); ++p) {
        auto const& particle = reco_ixn.part.sandreco[p];
        auto const p_i       = static_cast<int>(p);

        // truth match: resolves to a real SRTrueParticle with the same pdg
        UFW_ASSERT(!particle.truth.empty(), "nu[{}].part[{}]: no truth match", ixn_idx, p);
        UFW_ASSERT(particle.truth.size() == particle.truthOverlap.size(),
                   "nu[{}].part[{}]: truth/truthOverlap size mismatch", ixn_idx, p);
        auto const& true_part = resolve_true(truth, particle.truth[0]);
        UFW_ASSERT(true_part.pdg == particle.pdg, "nu[{}].part[{}]: pdg mismatch, reco {} vs true {}", ixn_idx, p,
                   particle.pdg, true_part.pdg);

        // recoobj <-> track/shower round trip
        if (particle.origRecoObjType == ::caf::RecoObjType::kTrack) {
          UFW_ASSERT(particle.recoobj.type == ::caf::SRRecoBaseID::kSANDTrackerTrack,
                     "nu[{}].part[{}]: recoobj.type isn't kSANDTrackerTrack", ixn_idx, p);
          UFW_ASSERT(particle.recoobj.ixn == ixn_idx_i, "nu[{}].part[{}]: recoobj.ixn doesn't match this interaction",
                     ixn_idx, p);
          UFW_ASSERT(particle.recoobj.irecoobj >= 0
                         && static_cast<std::size_t>(particle.recoobj.irecoobj) < sand_ixn.tracker.tracks.size(),
                     "nu[{}].part[{}]: recoobj.irecoobj out of range", ixn_idx, p);
          auto const& track = sand_ixn.tracker.tracks[static_cast<std::size_t>(particle.recoobj.irecoobj)];
          UFW_ASSERT(track.part.type == ::caf::SRRecoParticleID::kSandreco && track.part.ixn == ixn_idx_i
                         && track.part.ipart == p_i,
                     "nu[{}].part[{}]: matched track's part doesn't point back to this particle", ixn_idx, p);
        } else if (particle.origRecoObjType == ::caf::RecoObjType::kShower) {
          UFW_ASSERT(particle.recoobj.type == ::caf::SRRecoBaseID::kSANDTrackerShower,
                     "nu[{}].part[{}]: recoobj.type isn't kSANDTrackerShower", ixn_idx, p);
          UFW_ASSERT(particle.recoobj.ixn == ixn_idx_i, "nu[{}].part[{}]: recoobj.ixn doesn't match this interaction",
                     ixn_idx, p);
          UFW_ASSERT(particle.recoobj.irecoobj >= 0
                         && static_cast<std::size_t>(particle.recoobj.irecoobj) < sand_ixn.tracker.showers.size(),
                     "nu[{}].part[{}]: recoobj.irecoobj out of range", ixn_idx, p);
          auto const& shower = sand_ixn.tracker.showers[static_cast<std::size_t>(particle.recoobj.irecoobj)];
          UFW_ASSERT(shower.part.type == ::caf::SRRecoParticleID::kSandreco && shower.part.ixn == ixn_idx_i
                         && shower.part.ipart == p_i,
                     "nu[{}].part[{}]: matched shower's part doesn't point back to this particle", ixn_idx, p);
        }

        // parent/daughters round trip
        if (particle.parent >= 0) {
          UFW_ASSERT(static_cast<std::size_t>(particle.parent) < reco_ixn.part.sandreco.size(),
                     "nu[{}].part[{}]: parent index {} out of range", ixn_idx, p, particle.parent);
          auto const& parent = reco_ixn.part.sandreco[static_cast<std::size_t>(particle.parent)];
          bool const found   = std::any_of(parent.daughters.begin(), parent.daughters.end(),
                                           [&](unsigned int d) { return d == static_cast<unsigned int>(p); });
          UFW_ASSERT(found, "nu[{}].part[{}]: parent's daughters doesn't list it back", ixn_idx, p);
        }
      }
    }

    UFW_INFO("fast_reco_test: verified {} interaction(s)", common.ixn.sandreco.size());
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::fast_reco_test)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::fast_reco_test)
