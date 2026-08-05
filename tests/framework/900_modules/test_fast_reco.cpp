#include <caf/caf_wrapper.hpp>

#include <edep_reader/edep_reader.hpp>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace sand::test {

  /// Verifies that sand::common::fast_reco's output is internally consistent and
  /// round-trip navigable: every SRRecoParticle <-> SRTrack/SRShower link (recoobj/part)
  /// resolves back to the node it claims to, every truth match resolves to a real
  /// SRTrueParticle with the same pdg, and every parent/daughters link round-trips.
  ///
  /// It also re-derives, straight from sand::edep_reader, which particles were supposed to
  /// pass fast_reco's "has hits in the tracker" gate, and checks that exactly those got a reco
  /// object. The expectation is recomputed the slow way (EDEPTree::GetTrajectory) on
  /// purpose: it must not share code with the production gate, or it would verify nothing.
  struct fast_reco_test : public ufw::process {
    fast_reco_test();
    ~fast_reco_test() override;
    void run() override;

   private:
    std::size_t m_gated_in{};         ///< track-/shower-like particles that did get a reco object
    std::size_t m_gated_out{};        ///< track-/shower-like particles the tracker gate rejected
    std::size_t m_neutral_gated_in{}; ///< gamma/pi0 with a reco object (expected 0, see PLAN.md section 7)
  };

  fast_reco_test::fast_reco_test()
    : process({{"in_truth", "sand::caf::truth_branch_wrapper"},
               {"in_common", "sand::caf::common_reco_branch_wrapper"},
               {"in_nd", "sand::caf::nd_reco_branch_wrapper"}},
              {}) {}

  namespace {
    /// @return true for mu, pi+/-, K+/-, p. Deliberately a local copy of fast_reco's own table.
    bool is_track_like(int pdg) {
      int const abs_pdg = std::abs(pdg);
      return abs_pdg == 13 || abs_pdg == 211 || abs_pdg == 321 || abs_pdg == 2212;
    }

    /// @return true for e+/-, gamma, pi0. Deliberately a local copy of fast_reco's own table.
    bool is_shower_like(int pdg) {
      int const abs_pdg = std::abs(pdg);
      return abs_pdg == 11 || abs_pdg == 22 || abs_pdg == 111;
    }

    /// @return true for gamma, pi0: neutrals, which leave no hits of their own (PLAN.md section 7).
    bool is_neutral_shower_like(int pdg) {
      int const abs_pdg = std::abs(pdg);
      return abs_pdg == 22 || abs_pdg == 111;
    }

    bool has_tracker_hits(sand::edep_reader const& edep, ::caf::SRTrueParticle const& true_part) {
      if (true_part.G4ID < 0) {
        return false;
      }
      auto trj = edep.GetTrajectory(true_part.G4ID);
      return trj != edep.end()
          && (trj->HasHitInDetector(sand::subdetector_t::STT) || trj->HasHitInDetector(sand::subdetector_t::DRIFT));
    }

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
    auto const& edep   = instance<sand::edep_reader>();

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
      auto const& true_ixn = truth.nu[ixn_idx];
      auto const& reco_ixn = common.ixn.sandreco[ixn_idx];
      auto const& sand_ixn = nd.sand.ixn[ixn_idx];
      auto const ixn_idx_i = static_cast<int>(ixn_idx);
      auto const n_prim    = true_ixn.prim.size();

      UFW_ASSERT(reco_ixn.part.sandreco.size() == static_cast<std::size_t>(reco_ixn.part.nsandreco),
                 "nu[{}]: part.nsandreco ({}) doesn't match part.sandreco.size() ({})", ixn_idx,
                 reco_ixn.part.nsandreco, reco_ixn.part.sandreco.size());
      UFW_ASSERT(sand_ixn.tracker.tracks.size() == sand_ixn.tracker.ntracks,
                 "nu[{}]: tracker.ntracks ({}) doesn't match tracker.tracks.size() ({})", ixn_idx,
                 sand_ixn.tracker.ntracks, sand_ixn.tracker.tracks.size());
      UFW_ASSERT(sand_ixn.tracker.showers.size() == sand_ixn.tracker.nshowers,
                 "nu[{}]: tracker.nshowers ({}) doesn't match tracker.showers.size() ({})", ixn_idx,
                 sand_ixn.tracker.nshowers, sand_ixn.tracker.showers.size());

      UFW_ASSERT(reco_ixn.part.sandreco.size() == n_prim + true_ixn.sec.size(),
                 "nu[{}]: part.sandreco.size() ({}) doesn't match prim ({}) + sec ({})", ixn_idx,
                 reco_ixn.part.sandreco.size(), n_prim, true_ixn.sec.size());

      std::size_t n_with_track{};
      std::size_t n_with_shower{};

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

        UFW_ASSERT(particle.E == true_part.p.E,
                   "nu[{}].part[{}]: SRRecoParticle::E ({}) doesn't match true SRTrueParticle::p.E ({}) -- possible "
                   "GeV/MeV unit mismatch",
                   ixn_idx, p, particle.E, true_part.p.E);

        auto const& id           = particle.truth[0];
        auto const expected_type = (p < n_prim) ? ::caf::TrueParticleID::kPrimary : ::caf::TrueParticleID::kSecondary;
        auto const expected_part_idx = (p < n_prim) ? p : p - n_prim;
        UFW_ASSERT(id.ixn == ixn_idx_i && id.type == expected_type
                       && static_cast<std::size_t>(id.part) == expected_part_idx,
                   "nu[{}].part[{}]: truth[0] ({}, {}, {}) isn't the expected slot ({}, {}, {})", ixn_idx, p, id.ixn,
                   static_cast<int>(id.type), id.part, ixn_idx_i, static_cast<int>(expected_type), expected_part_idx);

        bool const trackish   = is_track_like(true_part.pdg);
        bool const showerish  = is_shower_like(true_part.pdg);
        bool const in_tracker = has_tracker_hits(edep, true_part);
        bool const expected   = in_tracker && (trackish || showerish);
        bool const has_obj    = static_cast<bool>(particle.recoobj);

        UFW_ASSERT(has_obj == expected,
                   "nu[{}].part[{}]: pdg {} (G4ID {}, hits in the tracker: {}) {} a reco object but {} one", ixn_idx, p,
                   particle.pdg, true_part.G4ID, in_tracker, expected ? "should have" : "should not have",
                   has_obj ? "has" : "has none");

        if (trackish || showerish) {
          ++(has_obj ? m_gated_in : m_gated_out);
        }
        if (has_obj && is_neutral_shower_like(particle.pdg)) {
          ++m_neutral_gated_in;
        }

        // origRecoObjType and recoobj are set together, from the same slot: neither can be set alone
        UFW_ASSERT(has_obj == (particle.origRecoObjType != ::caf::RecoObjType::kUnknownRecoObj),
                   "nu[{}].part[{}]: origRecoObjType ({}) and recoobj disagree on whether there is a reco object",
                   ixn_idx, p, static_cast<int>(particle.origRecoObjType));

        // recoobj <-> track/shower round trip
        if (particle.origRecoObjType == ::caf::RecoObjType::kTrack) {
          UFW_ASSERT(trackish, "nu[{}].part[{}]: pdg {} isn't track-like but got a track", ixn_idx, p, particle.pdg);
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
          UFW_ASSERT(track.E == true_part.p.E,
                     "nu[{}].part[{}]: SRTrack::E ({}) doesn't match true SRTrueParticle::p.E ({}) -- possible "
                     "GeV/MeV unit mismatch",
                     ixn_idx, p, track.E, true_part.p.E);
          UFW_ASSERT(track.Evis == track.E, "nu[{}].part[{}]: SRTrack::Evis ({}) doesn't match SRTrack::E ({})",
                     ixn_idx, p, track.Evis, track.E);
          ++n_with_track;
        } else if (particle.origRecoObjType == ::caf::RecoObjType::kShower) {
          UFW_ASSERT(showerish, "nu[{}].part[{}]: pdg {} isn't shower-like but got a shower", ixn_idx, p, particle.pdg);
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
          // Same units regression guard as above, on the nd branch: shower_from_true() also
          // copies true_part.p.E verbatim into SRShower::Evis.
          UFW_ASSERT(shower.Evis == true_part.p.E,
                     "nu[{}].part[{}]: SRShower::Evis ({}) doesn't match true SRTrueParticle::p.E ({}) -- possible "
                     "GeV/MeV unit mismatch",
                     ixn_idx, p, shower.Evis, true_part.p.E);
          ++n_with_shower;
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

      // Every track/shower is claimed by exactly one particle: the counts above already prove the
      // particle -> object direction is injective, so matching sizes close it into a bijection.
      // Catches duplicated or skipped track_idx/shower_idx, which the gate makes easy to get wrong.
      UFW_ASSERT(n_with_track == sand_ixn.tracker.tracks.size(),
                 "nu[{}]: {} particle(s) point to a track but tracker.tracks holds {}", ixn_idx, n_with_track,
                 sand_ixn.tracker.tracks.size());
      UFW_ASSERT(n_with_shower == sand_ixn.tracker.showers.size(),
                 "nu[{}]: {} particle(s) point to a shower but tracker.showers holds {}", ixn_idx, n_with_shower,
                 sand_ixn.tracker.showers.size());
    }

    UFW_INFO("fast_reco_test: verified {} interaction(s)", common.ixn.sandreco.size());
  }

  fast_reco_test::~fast_reco_test() {
    UFW_INFO("fast_reco_test: tracker gate let {} track-/shower-like particle(s) through, rejected {}", m_gated_in,
             m_gated_out);

    UFW_ASSERT(m_gated_in > 0, "no track-/shower-like particle passed the tracker gate over the whole job: gate stuck "
                               "at always-false, or wrong input file?");
    UFW_ASSERT(m_gated_out > 0, "no track-/shower-like particle was rejected by the tracker gate over the whole job: "
                                "gate inactive, or wrong input file?");

    if (m_neutral_gated_in != 0) {
      UFW_INFO("fast_reco_test: {} gamma/pi0 got a reco object -- unexpected, worth a look (PLAN.md section 7)",
               m_neutral_gated_in);
    }
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::fast_reco_test)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::fast_reco_test)
