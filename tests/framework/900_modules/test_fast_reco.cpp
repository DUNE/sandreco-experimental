#include <caf/caf_wrapper.hpp>

#include <edep_reader/edep_reader.hpp>
#include <spill_maps/spill_maps.hpp>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <TDatabasePDG.h>
#include <TParticlePDG.h>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace sand::test {

  /// Verifies that sand::common::fast_matching's output is internally consistent and
  /// round-trip navigable: every SRRecoParticle <-> SRTrack/SRShower link (recoobj/part)
  /// resolves back to the node it claims to, every truth match resolves to a real
  /// SRTrueParticle with the same pdg, and every parent/daughters link round-trips.
  ///
  /// It also re-derives, straight from sand::edep_reader, which particles were supposed to
  /// pass fast_reco's "has hits in the tracker" gate, and checks that exactly those got a reco
  /// object. The expectation is recomputed the slow way (EDEPTree::GetTrajectory) on
  /// purpose: it must not share code with the production gate, or it would verify nothing. The only shared piece
  /// is the G4ID -> edep-sim id conversion (G4IdMap), which test_truth_filler verifies on its own.
  struct fast_reco_test : public ufw::process {
    fast_reco_test();
    ~fast_reco_test() override;
    void configure(ufw::config const& cfg) override;
    void run() override;

   private:
    double m_min_energy_deposit{};                 ///< same `min_energy_deposit` [MeV] as sand::tracker::fast_reco
    std::size_t m_tracks_without_tracker_points{}; ///< tracks with E unknown (no trajectory point in the tracker)
    std::size_t m_gated_in{};                      ///< track-/shower-like particles that did get a reco object
    std::size_t m_gated_out{};                     ///< track-/shower-like particles the tracker gate rejected
    std::size_t m_neutral_gated_in{};              ///< gamma/pi0 with a reco object (expected 0)
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

    /// @return true for gamma, pi0: neutrals, which leave no hits of their own.
    bool is_neutral_shower_like(int pdg) {
      int const abs_pdg = std::abs(pdg);
      return abs_pdg == 22 || abs_pdg == 111;
    }

    bool has_tracker_hits(sand::edep_reader const& edep, EDEPTree::const_iterator trj) {
      return trj != edep.end()
          && (trj->HasHitInDetector(sand::subdetector_t::STT) || trj->HasHitInDetector(sand::subdetector_t::DRIFT));
    }

    /// @return the energy [MeV] deposited in the tracker. Deliberately a local copy of tracker::fast_reco's own.
    double total_energy_deposit(EDEPTrajectory const& trj) {
      double edep{};
      for (auto const& [det, hits] : trj.GetHitMap()) {
        if (det == sand::subdetector_t::STT || det == sand::subdetector_t::DRIFT) {
          for (auto const& hit : hits) {
            edep += hit.GetEnergyDeposit();
          }
        }
      }
      return edep;
    }

    /// The tracker hits a perfect tracker reco starts and ends with: the earliest and the latest by start time
    /// (on equal times, the first and the last met). Deliberately a local copy, found without sorting.
    struct tracker_ends {
      EDEPHit const* first{};
      EDEPHit const* last{};
      double length_mm{};
    };

    tracker_ends find_tracker_ends(EDEPTrajectory const& trj) {
      tracker_ends ends;
      for (auto const& [det, hits] : trj.GetHitMap()) {
        if (det != sand::subdetector_t::STT && det != sand::subdetector_t::DRIFT) {
          continue;
        }
        for (auto const& hit : hits) {
          auto const t = hit.GetStart().T();
          if (ends.first == nullptr || t < ends.first->GetStart().T()) {
            ends.first = &hit;
          }
          if (ends.last == nullptr || t >= ends.last->GetStart().T()) {
            ends.last = &hit;
          }
          ends.length_mm += hit.GetTrackLength();
        }
      }
      return ends;
    }

    /// edep-sim positions are in mm, CAF positions in cm (same expression as the reco, so equality is exact).
    bool same_position_cm(::caf::SRVector3D const& reco, sand::vec_4d const& edep_mm) {
      return reco.x == static_cast<float>(edep_mm.X() * 0.1) && reco.y == static_cast<float>(edep_mm.Y() * 0.1)
          && reco.z == static_cast<float>(edep_mm.Z() * 0.1);
    }

    /// Sums may be done in a different order than in the reco: compare with a relative tolerance.
    bool close(double a, double b) { return std::abs(a - b) <= 1e-5 * std::max(std::abs(a), std::abs(b)); }

    /// The earliest and the latest trajectory point in the tracker (STT/drift), by time; on equal times the first
    /// met for `first`, the last met for `last`. Deliberately a local copy of tracker::fast_reco's own.
    struct tracker_point_ends {
      EDEPTrajectoryPoint const* first{};
      EDEPTrajectoryPoint const* last{};
    };

    tracker_point_ends find_tracker_point_ends(EDEPTrajectory const& trj) {
      tracker_point_ends ends;
      for (auto const& [det, points] : trj.GetTrajectoryPoints()) {
        if (det != sand::subdetector_t::STT && det != sand::subdetector_t::DRIFT) {
          continue;
        }
        for (auto const& point : points) {
          auto const t = point.GetPosition().T();
          if (ends.first == nullptr || t < ends.first->GetPosition().T()) {
            ends.first = &point;
          }
          if (ends.last == nullptr || t >= ends.last->GetPosition().T()) {
            ends.last = &point;
          }
        }
      }
      return ends;
    }

    /// @return true if `dir` is the unit vector along (x, y, z), within 1e-4 per component.
    bool same_direction(::caf::SRVector3D const& dir, double x, double y, double z) {
      double const norm = std::sqrt(x * x + y * y + z * z);
      return norm > 0. && std::abs(dir.x - x / norm) < 1e-4 && std::abs(dir.y - y / norm) < 1e-4
          && std::abs(dir.z - z / norm) < 1e-4;
    }

    bool same_direction(::caf::SRVector3D const& dir, sand::mom_3d const& p) {
      return same_direction(dir, p.x(), p.y(), p.z());
    }

    bool same_direction(::caf::SRVector3D const& dir, EDEPHit const& hit) {
      return same_direction(dir, hit.GetStop().X() - hit.GetStart().X(), hit.GetStop().Y() - hit.GetStart().Y(),
                            hit.GetStop().Z() - hit.GetStart().Z());
    }

    bool is_stopped(sand::mom_3d const& p) { return std::sqrt(p.Mag2()) <= 1e-6; }

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

  void fast_reco_test::configure(ufw::config const& cfg) {
    process::configure(cfg);
    m_min_energy_deposit = cfg.value("min_energy_deposit", 0.);
  }

  void fast_reco_test::run() {
    auto const& truth  = get<sand::caf::truth_branch_wrapper>("in_truth");
    auto const& common = get<sand::caf::common_reco_branch_wrapper>("in_common");
    auto const& nd     = get<sand::caf::nd_reco_branch_wrapper>("in_nd");
    auto const& edep   = instance<sand::edep_reader>();

    auto const& track_ids = instance<sand::spill_maps>();

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

    auto const& tracker = nd.sand.tracker;
    UFW_ASSERT(tracker.tracks.size() == tracker.ntracks,
               "tracker.ntracks ({}) doesn't match tracker.tracks.size() ({})", tracker.ntracks, tracker.tracks.size());
    UFW_ASSERT(tracker.showers.size() == tracker.nshowers,
               "tracker.nshowers ({}) doesn't match tracker.showers.size() ({})", tracker.nshowers,
               tracker.showers.size());

    std::size_t n_with_track{};
    std::size_t n_with_shower{};

    for (std::size_t ixn_idx{}; ixn_idx != common.ixn.sandreco.size(); ++ixn_idx) {
      auto const& true_ixn = truth.nu[ixn_idx];
      auto const& reco_ixn = common.ixn.sandreco[ixn_idx];
      auto const ixn_idx_i = static_cast<int>(ixn_idx);
      auto const n_prim    = true_ixn.prim.size();

      UFW_ASSERT(reco_ixn.part.sandreco.size() == static_cast<std::size_t>(reco_ixn.part.nsandreco),
                 "nu[{}]: part.nsandreco ({}) doesn't match part.sandreco.size() ({})", ixn_idx,
                 reco_ixn.part.nsandreco, reco_ixn.part.sandreco.size());
      UFW_ASSERT(reco_ixn.part.sandreco.size() == n_prim + true_ixn.sec.size(),
                 "nu[{}]: part.sandreco.size() ({}) doesn't match prim ({}) + sec ({})", ixn_idx,
                 reco_ixn.part.sandreco.size(), n_prim, true_ixn.sec.size());

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

        // G4ID -> edep-sim trajectory: must be the same particle, or every lookup by G4ID reads the wrong hits
        auto const trj = edep.GetTrajectory(track_ids.at(ixn_idx).to_spill_id(true_part.G4ID));
        if (trj != edep.end() && !trj->GetTrajectoryPoints().empty()) {
          auto const start = trj->GetTrajectoryPointsVect().front().GetPosition();
          UFW_ASSERT(trj->GetPDGCode() == true_part.pdg && std::abs(start.X() * 0.1 - true_part.start_pos.x) < 1e-3
                         && std::abs(start.Y() * 0.1 - true_part.start_pos.y) < 1e-3
                         && std::abs(start.Z() * 0.1 - true_part.start_pos.z) < 1e-3,
                     "nu[{}].part[{}]: G4ID {} resolves to edep-sim trajectory pdg {} at ({}, {}, {}) cm, but the true "
                     "particle is pdg {} at ({}, {}, {}) cm",
                     ixn_idx, p, true_part.G4ID, trj->GetPDGCode(), start.X() * 0.1, start.Y() * 0.1, start.Z() * 0.1,
                     true_part.pdg, true_part.start_pos.x, true_part.start_pos.y, true_part.start_pos.z);
        }

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
        bool const in_tracker = has_tracker_hits(edep, trj) && total_energy_deposit(*trj) >= m_min_energy_deposit;
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
                         && static_cast<std::size_t>(particle.recoobj.irecoobj) < tracker.tracks.size(),
                     "nu[{}].part[{}]: recoobj.irecoobj out of range", ixn_idx, p);
          auto const& track = tracker.tracks[static_cast<std::size_t>(particle.recoobj.irecoobj)];
          UFW_ASSERT(track.part.type == ::caf::SRRecoParticleID::kSandreco && track.part.ixn == ixn_idx_i
                         && track.part.ipart == p_i,
                     "nu[{}].part[{}]: matched track's part doesn't point back to this particle", ixn_idx, p);
          // start/end/time/length/Evis come from the tracker hits only, never from the whole trajectory
          auto const ends = find_tracker_ends(*trj);
          UFW_ASSERT(ends.first != nullptr, "nu[{}].part[{}]: track without tracker hits", ixn_idx, p);
          UFW_ASSERT(same_position_cm(track.start, ends.first->GetStart()),
                     "nu[{}].part[{}]: track start ({}, {}, {}) isn't the start of its first tracker hit {}", ixn_idx,
                     p, track.start.x, track.start.y, track.start.z, ends.first->GetStart());
          UFW_ASSERT(same_position_cm(track.end, ends.last->GetStop()),
                     "nu[{}].part[{}]: track end ({}, {}, {}) isn't the stop of its last tracker hit {}", ixn_idx, p,
                     track.end.x, track.end.y, track.end.z, ends.last->GetStop());
          UFW_ASSERT(track.time == ends.first->GetStart().T(),
                     "nu[{}].part[{}]: track time {} isn't the time of its first tracker hit {}", ixn_idx, p,
                     track.time, ends.first->GetStart().T());
          UFW_ASSERT(close(track.len_cm, ends.length_mm * 0.1),
                     "nu[{}].part[{}]: track length {} cm isn't the summed length of its tracker hits {} cm", ixn_idx,
                     p, track.len_cm, ends.length_mm * 0.1);
          UFW_ASSERT(close(track.Evis, total_energy_deposit(*trj)),
                     "nu[{}].part[{}]: SRTrack::Evis ({} MeV) isn't the energy deposited in the tracker ({} MeV)",
                     ixn_idx, p, track.Evis, total_energy_deposit(*trj));

          // dir/E at the tracker entry and enddir at the tracker exit, from the trajectory points in the tracker;
          // without them, directions from the first/last hit segment and E unknown (NaN)
          auto const points = find_tracker_point_ends(*trj);
          if (points.first == nullptr) {
            ++m_tracks_without_tracker_points;
            UFW_ASSERT(std::isnan(track.E), "nu[{}].part[{}]: no trajectory point in the tracker, but E = {} MeV",
                       ixn_idx, p, track.E);
            UFW_ASSERT(same_direction(track.dir, *ends.first) && same_direction(track.enddir, *ends.last),
                       "nu[{}].part[{}]: no trajectory point in the tracker, but dir/enddir aren't the first/last "
                       "hit's direction",
                       ixn_idx, p);
          } else {
            auto const* pdg_info = TDatabasePDG::Instance()->GetParticle(true_part.pdg);
            double const mass    = pdg_info != nullptr ? pdg_info->Mass() * 1000. : 0.; // GeV -> MeV
            auto const& p_in     = points.first->GetMomentum();                         // MeV
            double const e_in    = std::sqrt(p_in.Mag2() + mass * mass);
            // SRTrack::E is in MeV, at the tracker entry: never more than the true energy at the vertex (GeV)
            UFW_ASSERT(close(track.E, e_in),
                       "nu[{}].part[{}]: SRTrack::E ({} MeV) isn't the energy at the tracker entry ({} MeV)", ixn_idx,
                       p, track.E, e_in);
            UFW_ASSERT(track.E <= true_part.p.E * 1000. * (1. + 1e-5),
                       "nu[{}].part[{}]: SRTrack::E ({} MeV) is more than the true energy at the vertex ({} MeV)",
                       ixn_idx, p, track.E, true_part.p.E * 1000.);
            if (!is_stopped(p_in)) {
              UFW_ASSERT(same_direction(track.dir, p_in),
                         "nu[{}].part[{}]: track dir ({}, {}, {}) isn't the momentum direction at the tracker entry",
                         ixn_idx, p, track.dir.x, track.dir.y, track.dir.z);
            }
            if (auto const& p_out = points.last->GetMomentum(); !is_stopped(p_out)) {
              UFW_ASSERT(same_direction(track.enddir, p_out),
                         "nu[{}].part[{}]: track enddir ({}, {}, {}) isn't the momentum direction at the tracker exit",
                         ixn_idx, p, track.enddir.x, track.enddir.y, track.enddir.z);
            }
          }
          ++n_with_track;
        } else if (particle.origRecoObjType == ::caf::RecoObjType::kShower) {
          UFW_ASSERT(showerish, "nu[{}].part[{}]: pdg {} isn't shower-like but got a shower", ixn_idx, p, particle.pdg);
          UFW_ASSERT(particle.recoobj.type == ::caf::SRRecoBaseID::kSANDTrackerShower,
                     "nu[{}].part[{}]: recoobj.type isn't kSANDTrackerShower", ixn_idx, p);
          UFW_ASSERT(particle.recoobj.ixn == ixn_idx_i, "nu[{}].part[{}]: recoobj.ixn doesn't match this interaction",
                     ixn_idx, p);
          UFW_ASSERT(particle.recoobj.irecoobj >= 0
                         && static_cast<std::size_t>(particle.recoobj.irecoobj) < tracker.showers.size(),
                     "nu[{}].part[{}]: recoobj.irecoobj out of range", ixn_idx, p);
          auto const& shower = tracker.showers[static_cast<std::size_t>(particle.recoobj.irecoobj)];
          UFW_ASSERT(shower.part.type == ::caf::SRRecoParticleID::kSandreco && shower.part.ixn == ixn_idx_i
                         && shower.part.ipart == p_i,
                     "nu[{}].part[{}]: matched shower's part doesn't point back to this particle", ixn_idx, p);
          // start/time/Evis come from the tracker hits only
          auto const ends = find_tracker_ends(*trj);
          UFW_ASSERT(ends.first != nullptr, "nu[{}].part[{}]: shower without tracker hits", ixn_idx, p);
          UFW_ASSERT(same_position_cm(shower.start, ends.first->GetStart()),
                     "nu[{}].part[{}]: shower start ({}, {}, {}) isn't the start of its first tracker hit {}", ixn_idx,
                     p, shower.start.x, shower.start.y, shower.start.z, ends.first->GetStart());
          UFW_ASSERT(shower.time == ends.first->GetStart().T(),
                     "nu[{}].part[{}]: shower time {} isn't the time of its first tracker hit {}", ixn_idx, p,
                     shower.time, ends.first->GetStart().T());
          UFW_ASSERT(close(shower.Evis, total_energy_deposit(*trj)),
                     "nu[{}].part[{}]: SRShower::Evis ({} MeV) isn't the energy deposited in the tracker ({} MeV)",
                     ixn_idx, p, shower.Evis, total_energy_deposit(*trj));

          // direction at the tracker entry, else from the first hit segment
          auto const points = find_tracker_point_ends(*trj);
          if (points.first != nullptr && !is_stopped(points.first->GetMomentum())) {
            UFW_ASSERT(same_direction(shower.direction, points.first->GetMomentum()),
                       "nu[{}].part[{}]: shower direction isn't the momentum direction at the tracker entry", ixn_idx,
                       p);
          } else if (points.first == nullptr) {
            UFW_ASSERT(same_direction(shower.direction, *ends.first),
                       "nu[{}].part[{}]: no trajectory point in the tracker, but the shower direction isn't the "
                       "first hit's",
                       ixn_idx, p);
          }
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
    }

    UFW_ASSERT(n_with_track == tracker.tracks.size(), "{} particle(s) point to a track but tracker.tracks holds {}",
               n_with_track, tracker.tracks.size());
    UFW_ASSERT(n_with_shower == tracker.showers.size(), "{} particle(s) point to a shower but tracker.showers holds {}",
               n_with_shower, tracker.showers.size());

    UFW_INFO("fast_reco_test: verified {} interaction(s)", common.ixn.sandreco.size());
  }

  fast_reco_test::~fast_reco_test() {
    UFW_INFO("fast_reco_test: {} track(s) without trajectory points in the tracker (E unknown)",
             m_tracks_without_tracker_points);
    UFW_INFO("fast_reco_test: tracker gate let {} track-/shower-like particle(s) through, rejected {}", m_gated_in,
             m_gated_out);

    UFW_ASSERT(m_gated_in > 0, "no track-/shower-like particle passed the tracker gate over the whole job: gate stuck "
                               "at always-false, or wrong input file?");
    UFW_ASSERT(m_gated_out > 0, "no track-/shower-like particle was rejected by the tracker gate over the whole job: "
                                "gate inactive, or wrong input file?");

    if (m_neutral_gated_in != 0) {
      UFW_INFO("fast_reco_test: {} gamma/pi0 got a reco object -- unexpected, worth a look", m_neutral_gated_in);
    }
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::fast_reco_test)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::fast_reco_test)
