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
#include <vector>

namespace sand::test {

  /// Verifies the output of the fast reco chain (tracker reco, matching, vertexing, pid):
  /// 1. counters and the index alignment of common.ixn and nd.sand.ixn;
  /// 2. the gate: a true track-/shower-like particle has a reco particle if and only if it left at least
  ///    `min_energy_deposit` in the tracker, recomputed from sand::edep_reader without sharing code with the reco;
  /// 3. the links: particle j <-> trkmatch[j] <-> its tracker object, with the same truth, every object used once;
  /// 4. the tracker objects: position, time, length, Evis and enddir from the hits, dir and E at the tracker entry;
  /// 5. the parentage: the parent of a reco particle is its nearest visible true ancestor, and daughters mirror it.
  struct fast_reco_test : public ufw::process {
    fast_reco_test();
    ~fast_reco_test() override;
    void configure(ufw::config const& cfg) override;
    void run() override;

   private:
    double m_min_energy_deposit{}; ///< same `min_energy_deposit` [MeV] as sand::fast::tracker::reco
    std::size_t m_gated_in{};      ///< track-/shower-like true particles with a reco particle
    std::size_t m_gated_out{};     ///< track-/shower-like true particles the gate rejected
  };

  fast_reco_test::fast_reco_test()
    : process({{"in_truth", "sand::caf::truth_branch_wrapper"},
               {"in_common", "sand::caf::common_reco_branch_wrapper"},
               {"in_nd", "sand::caf::nd_reco_branch_wrapper"}},
              {}) {}

  namespace {
    /// @return true for mu, pi+/-, K+/-, p. Deliberately a local copy of the reco's own table.
    bool is_track_like(int pdg) {
      int const abs_pdg = std::abs(pdg);
      return abs_pdg == 13 || abs_pdg == 211 || abs_pdg == 321 || abs_pdg == 2212;
    }

    /// @return true for e+/-, gamma, pi0. Deliberately a local copy of the reco's own table.
    bool is_shower_like(int pdg) {
      int const abs_pdg = std::abs(pdg);
      return abs_pdg == 11 || abs_pdg == 22 || abs_pdg == 111;
    }

    bool is_tracker(sand::subdetector_t det) {
      return det == sand::subdetector_t::STT || det == sand::subdetector_t::DRIFT;
    }

    /// The tracker hits of a trajectory, found without sorting: the earliest and the latest by start time (on equal
    /// times, the first and the last met), the summed length and the deposited energy.
    struct tracker_hits {
      EDEPHit const* first{};
      EDEPHit const* last{};
      double length_mm{};
      double energy{}; ///< [MeV]
    };

    tracker_hits find_tracker_hits(EDEPTrajectory const& trj) {
      tracker_hits th;
      for (auto const& [det, hits] : trj.GetHitMap()) {
        if (!is_tracker(det)) {
          continue;
        }
        for (auto const& hit : hits) {
          auto const t = hit.GetStart().T();
          if (th.first == nullptr || t < th.first->GetStart().T()) {
            th.first = &hit;
          }
          if (th.last == nullptr || t >= th.last->GetStart().T()) {
            th.last = &hit;
          }
          th.length_mm += hit.GetTrackLength();
          th.energy += hit.GetEnergyDeposit();
        }
      }
      return th;
    }

    /// The earliest trajectory point in the tracker, null if edep-sim stored none there
    EDEPTrajectoryPoint const* find_entry_point(EDEPTrajectory const& trj) {
      EDEPTrajectoryPoint const* entry{};
      for (auto const& [det, points] : trj.GetTrajectoryPoints()) {
        if (!is_tracker(det)) {
          continue;
        }
        for (auto const& point : points) {
          if (entry == nullptr || point.GetPosition().T() < entry->GetPosition().T()) {
            entry = &point;
          }
        }
      }
      return entry;
    }

    /// edep-sim positions are in mm, CAF positions in cm (same expression as the reco, so equality is exact).
    bool same_position_cm(::caf::SRVector3D const& reco, sand::vec_4d const& edep_mm) {
      return reco.x == static_cast<float>(edep_mm.X() * 0.1) && reco.y == static_cast<float>(edep_mm.Y() * 0.1)
          && reco.z == static_cast<float>(edep_mm.Z() * 0.1);
    }

    /// Sums may be done in a different order than in the reco: compare with a relative tolerance.
    bool close(double a, double b) { return std::abs(a - b) <= 1e-5 * std::max(std::abs(a), std::abs(b)); }

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

    bool is_particle(::caf::TrueParticleID const& id) {
      return id.type == ::caf::TrueParticleID::kPrimary || id.type == ::caf::TrueParticleID::kSecondary;
    }

    bool same_particle(::caf::TrueParticleID const& a, ::caf::TrueParticleID const& b) {
      return a.ixn == b.ixn && a.type == b.type && a.part == b.part;
    }

    bool same_reco_particle(::caf::SRRecoParticleID const& id, int ixn, int ipart) {
      return id.type == ::caf::SRRecoParticleID::kSandreco && id.ixn == ixn && id.ipart == ipart;
    }

    ::caf::SRTrueParticle const& resolve_true(::caf::SRTruthBranch const& truth, ::caf::TrueParticleID const& id) {
      UFW_ASSERT(id.ixn >= 0 && static_cast<std::size_t>(id.ixn) < truth.nu.size(),
                 "TrueParticleID::ixn {} out of range", id.ixn);
      auto const& ixn = truth.nu[static_cast<std::size_t>(id.ixn)];
      UFW_ASSERT(is_particle(id), "Unexpected TrueParticleID::type {}", static_cast<int>(id.type));
      auto const& collection = (id.type == ::caf::TrueParticleID::kPrimary) ? ixn.prim : ixn.sec;
      UFW_ASSERT(id.part >= 0 && static_cast<std::size_t>(id.part) < collection.size(),
                 "TrueParticleID::part {} out of range", id.part);
      return collection[static_cast<std::size_t>(id.part)];
    }

    /// Tracker reco: position, time, length, Evis and enddir from the hits; dir and E at the entry point (E = NaN and
    /// dir from the first hit without one)
    void check_track(::caf::SRTrack const& track, EDEPTrajectory const& trj, ::caf::SRTrueParticle const& true_part,
                     std::size_t i, std::size_t j) {
      auto const hits = find_tracker_hits(trj);
      UFW_ASSERT(hits.first != nullptr, "ixn[{}].part[{}]: track without tracker hits", i, j);
      UFW_ASSERT(same_position_cm(track.start, hits.first->GetStart()),
                 "ixn[{}].part[{}]: track start isn't the start of its first tracker hit", i, j);
      UFW_ASSERT(same_position_cm(track.end, hits.last->GetStop()),
                 "ixn[{}].part[{}]: track end isn't the stop of its last tracker hit", i, j);
      UFW_ASSERT(track.time == hits.first->GetStart().T(),
                 "ixn[{}].part[{}]: track time isn't the time of its first tracker hit", i, j);
      UFW_ASSERT(close(track.len_cm, hits.length_mm * 0.1),
                 "ixn[{}].part[{}]: track length {} cm isn't the summed length of its tracker hits {} cm", i, j,
                 track.len_cm, hits.length_mm * 0.1);
      UFW_ASSERT(close(track.Evis, hits.energy), "ixn[{}].part[{}]: Evis {} MeV isn't the tracker deposit {} MeV", i,
                 j, track.Evis, hits.energy);
      UFW_ASSERT(same_direction(track.enddir, *hits.last),
                 "ixn[{}].part[{}]: enddir isn't the direction of the last tracker hit", i, j);

      auto const* entry = find_entry_point(trj);
      if (entry == nullptr) {
        UFW_ASSERT(std::isnan(track.E), "ixn[{}].part[{}]: no entry point, but E = {} MeV", i, j, track.E);
        UFW_ASSERT(same_direction(track.dir, *hits.first),
                   "ixn[{}].part[{}]: no entry point, but dir isn't the first hit's direction", i, j);
        return;
      }
      auto const* pdg_info = TDatabasePDG::Instance()->GetParticle(true_part.pdg);
      double const mass    = pdg_info != nullptr ? pdg_info->Mass() * 1000. : 0.; // GeV -> MeV
      auto const& p_in     = entry->GetMomentum();                                 // MeV
      UFW_ASSERT(close(track.E, std::sqrt(p_in.Mag2() + mass * mass)),
                 "ixn[{}].part[{}]: E {} MeV isn't the energy at the tracker entry", i, j, track.E);
      UFW_ASSERT(track.E <= true_part.p.E * 1000. * (1. + 1e-5),
                 "ixn[{}].part[{}]: E {} MeV is more than the true energy at the vertex {} MeV", i, j, track.E,
                 true_part.p.E * 1000.);
      if (!is_stopped(p_in)) {
        UFW_ASSERT(same_direction(track.dir, p_in), "ixn[{}].part[{}]: dir isn't the momentum at the tracker entry",
                   i, j);
      }
    }

    /// Tracker reco: start, time and Evis from the hits; direction at the entry point, else from the first hit
    void check_shower(::caf::SRShower const& shower, EDEPTrajectory const& trj, std::size_t i, std::size_t j) {
      auto const hits = find_tracker_hits(trj);
      UFW_ASSERT(hits.first != nullptr, "ixn[{}].part[{}]: shower without tracker hits", i, j);
      UFW_ASSERT(same_position_cm(shower.start, hits.first->GetStart()),
                 "ixn[{}].part[{}]: shower start isn't the start of its first tracker hit", i, j);
      UFW_ASSERT(shower.time == hits.first->GetStart().T(),
                 "ixn[{}].part[{}]: shower time isn't the time of its first tracker hit", i, j);
      UFW_ASSERT(close(shower.Evis, hits.energy), "ixn[{}].part[{}]: Evis {} MeV isn't the tracker deposit {} MeV", i,
                 j, shower.Evis, hits.energy);

      auto const* entry = find_entry_point(trj);
      if (entry == nullptr) {
        UFW_ASSERT(same_direction(shower.direction, *hits.first),
                   "ixn[{}].part[{}]: no entry point, but direction isn't the first hit's", i, j);
      } else if (!is_stopped(entry->GetMomentum())) {
        UFW_ASSERT(same_direction(shower.direction, entry->GetMomentum()),
                   "ixn[{}].part[{}]: direction isn't the momentum at the tracker entry", i, j);
      }
    }
  } // namespace

  void fast_reco_test::configure(ufw::config const& cfg) {
    process::configure(cfg);
    m_min_energy_deposit = cfg.value("min_energy_deposit", 0.);
  }

  void fast_reco_test::run() {
    auto const& truth   = get<sand::caf::truth_branch_wrapper>("in_truth");
    auto const& common  = get<sand::caf::common_reco_branch_wrapper>("in_common");
    auto const& nd      = get<sand::caf::nd_reco_branch_wrapper>("in_nd");
    auto const& edep    = instance<sand::edep_reader>();
    auto const& id_maps = instance<sand::spill_maps>();
    auto const& tracker = nd.sand.tracker;

    // The edep-sim trajectory of a true particle
    auto const trajectory = [&](::caf::TrueParticleID const& id) -> EDEPTrajectory const& {
      auto const& true_part = resolve_true(truth, id);
      auto const trj = edep.GetTrajectory(id_maps.at(static_cast<std::size_t>(id.ixn)).to_spill_id(true_part.G4ID));
      UFW_ASSERT(trj != edep.end(), "true particle G4ID {} of interaction {} has no edep-sim trajectory",
                 true_part.G4ID, id.ixn);
      return *trj;
    };

    // 1. Counters and alignment
    UFW_ASSERT(common.ixn.sandreco.size() == common.ixn.nsandreco, "common.ixn.nsandreco doesn't match its size");
    UFW_ASSERT(nd.sand.ixn.size() == nd.sand.nixn, "nd.sand.nixn doesn't match its size");
    UFW_ASSERT(tracker.tracks.size() == tracker.ntracks, "tracker.ntracks doesn't match its size");
    UFW_ASSERT(tracker.showers.size() == tracker.nshowers, "tracker.nshowers doesn't match its size");
    UFW_ASSERT(common.ixn.sandreco.size() == nd.sand.ixn.size(),
               "common.ixn.sandreco ({}) and nd.sand.ixn ({}) are not index-aligned", common.ixn.sandreco.size(),
               nd.sand.ixn.size());

    // How many particles point to each tracker object: exactly one at the end
    std::vector<int> track_users(tracker.tracks.size());
    std::vector<int> shower_users(tracker.showers.size());

    for (std::size_t i{}; i != common.ixn.sandreco.size(); ++i) {
      auto const& reco_ixn  = common.ixn.sandreco[i];
      auto const& particles = reco_ixn.part.sandreco;
      auto const& sand_ixn  = nd.sand.ixn[i];
      auto const& trkmatch  = sand_ixn.trkmatch;
      auto const& true_ixn  = truth.nu.at(reco_ixn.truth.at(0));
      auto const i_int      = static_cast<int>(i);

      UFW_ASSERT(particles.size() == static_cast<std::size_t>(reco_ixn.part.nsandreco),
                 "ixn[{}]: part.nsandreco doesn't match its size", i);
      UFW_ASSERT(trkmatch.size() == sand_ixn.ntrkmatch, "ixn[{}]: ntrkmatch doesn't match its size", i);
      UFW_ASSERT(particles.size() == trkmatch.size(), "ixn[{}]: {} reco particles but {} assns", i, particles.size(),
                 trkmatch.size());

      // Which true particles were seen: true particle -> reco particle j, -1 if invisible
      std::vector<int> prim_to_j(true_ixn.prim.size(), -1);
      std::vector<int> sec_to_j(true_ixn.sec.size(), -1);
      auto const reco_index = [&](::caf::TrueParticleID const& id) -> int& {
        auto& table = (id.type == ::caf::TrueParticleID::kPrimary) ? prim_to_j : sec_to_j;
        return table.at(static_cast<std::size_t>(id.part));
      };

      for (std::size_t j{}; j != particles.size(); ++j) {
        auto const& particle = particles[j];
        auto const& assn     = trkmatch[j];
        auto const j_int     = static_cast<int>(j);

        // 3. Links: particle j <-> trkmatch[j] <-> its object, all with the same truth
        UFW_ASSERT(particle.truth.size() == 1 && particle.truthOverlap.size() == 1,
                   "ixn[{}].part[{}]: expected exactly one truth match", i, j);
        auto const& id = particle.truth[0];
        UFW_ASSERT(same_particle(id, assn.trk.truth.at(0)), "ixn[{}].part[{}]: truth differs from its assn's", i, j);
        UFW_ASSERT(reco_index(id) == -1, "ixn[{}].part[{}]: true particle already has a reco particle", i, j);
        reco_index(id) = j_int;

        auto const& true_part = resolve_true(truth, id);
        UFW_ASSERT(particle.pdg == true_part.pdg, "ixn[{}].part[{}]: pdg {} isn't the true {}", i, j, particle.pdg,
                   true_part.pdg);
        UFW_ASSERT(particle.E == true_part.p.E, "ixn[{}].part[{}]: E {} isn't the true {} GeV (unit mismatch?)", i,
                   j, particle.E, true_part.p.E);
        UFW_ASSERT(particle.primary == (id.type == ::caf::TrueParticleID::kPrimary),
                   "ixn[{}].part[{}]: primary flag disagrees with the truth", i, j);

        UFW_ASSERT(particle.recoobj.type == ::caf::SRRecoBaseID::kSANDAssn && particle.recoobj.ixn == i_int
                       && particle.recoobj.irecoobj == j_int,
                   "ixn[{}].part[{}]: recoobj isn't trkmatch[{}] of this interaction", i, j, j);
        UFW_ASSERT(same_reco_particle(assn.part, i_int, j_int), "ixn[{}].part[{}]: assn.part doesn't point back", i, j);
        UFW_ASSERT(assn.constituents.size() == 1, "ixn[{}].part[{}]: {} constituents, expected 1 (tracker only)", i, j,
                   assn.constituents.size());

        auto const& constituent = assn.constituents.front();
        auto const idx          = static_cast<std::size_t>(constituent.idx);
        auto const& trj         = trajectory(id);
        if (constituent.type == ::caf::SRRecoBaseID::kSANDTrackerTrack) {
          UFW_ASSERT(idx < tracker.tracks.size(), "ixn[{}].part[{}]: track index out of range", i, j);
          auto const& track = tracker.tracks[idx];
          UFW_ASSERT(particle.origRecoObjType == ::caf::RecoObjType::kTrack && is_track_like(particle.pdg),
                     "ixn[{}].part[{}]: pdg {} got a track", i, j, particle.pdg);
          UFW_ASSERT(same_reco_particle(track.part, i_int, j_int) && same_particle(track.truth.at(0), id),
                     "ixn[{}].part[{}]: its track doesn't point back, or has another truth", i, j);
          ++track_users[idx];
          // 4. Tracker objects
          check_track(track, trj, true_part, i, j);
        } else if (constituent.type == ::caf::SRRecoBaseID::kSANDTrackerShower) {
          UFW_ASSERT(idx < tracker.showers.size(), "ixn[{}].part[{}]: shower index out of range", i, j);
          auto const& shower = tracker.showers[idx];
          UFW_ASSERT(particle.origRecoObjType == ::caf::RecoObjType::kShower && is_shower_like(particle.pdg),
                     "ixn[{}].part[{}]: pdg {} got a shower", i, j, particle.pdg);
          UFW_ASSERT(same_reco_particle(shower.part, i_int, j_int) && same_particle(shower.truth.at(0), id),
                     "ixn[{}].part[{}]: its shower doesn't point back, or has another truth", i, j);
          ++shower_users[idx];
          check_shower(shower, trj, i, j);
        } else {
          UFW_ERROR("ixn[{}].part[{}]: unexpected constituent type {}", i, j, static_cast<int>(constituent.type));
        }
      }

      // 2. Gate: a track-/shower-like true particle is seen iff it left enough energy in the tracker
      for (auto const type : {::caf::TrueParticleID::kPrimary, ::caf::TrueParticleID::kSecondary}) {
        auto const n = (type == ::caf::TrueParticleID::kPrimary) ? true_ixn.prim.size() : true_ixn.sec.size();
        for (std::size_t k{}; k != n; ++k) {
          ::caf::TrueParticleID const id{static_cast<int>(reco_ixn.truth.at(0)), type, static_cast<int>(k)};
          auto const& true_part = resolve_true(truth, id);
          bool const candidate  = is_track_like(true_part.pdg) || is_shower_like(true_part.pdg);
          auto const hits       = find_tracker_hits(trajectory(id));
          bool const expected   = candidate && hits.first != nullptr && hits.energy >= m_min_energy_deposit;
          bool const seen       = reco_index(id) >= 0;
          UFW_ASSERT(seen == expected, "ixn[{}]: true particle pdg {} (G4ID {}, {} MeV in the tracker) {} be seen", i,
                     true_part.pdg, true_part.G4ID, hits.energy, expected ? "should" : "should not");
          if (candidate) {
            ++(seen ? m_gated_in : m_gated_out);
          }
        }
      }

      // 5. Parentage: the parent is the nearest visible true ancestor; daughters mirror it
      std::size_t n_daughters{};
      for (std::size_t j{}; j != particles.size(); ++j) {
        auto ancestor = resolve_true(truth, particles[j].truth[0]).parentID;
        while (is_particle(ancestor) && reco_index(ancestor) < 0) {
          ancestor = resolve_true(truth, ancestor).parentID;
        }
        int const expected = is_particle(ancestor) ? reco_index(ancestor) : -1;
        UFW_ASSERT(particles[j].parent == expected, "ixn[{}].part[{}]: parent {} isn't the nearest visible ancestor {}",
                   i, j, particles[j].parent, expected);
        for (auto const d : particles[j].daughters) {
          UFW_ASSERT(d < particles.size() && particles[d].parent == static_cast<int>(j),
                     "ixn[{}].part[{}]: daughter {} doesn't have it as parent", i, j, d);
        }
        n_daughters += particles[j].daughters.size();
      }
      auto const n_with_parent =
          std::count_if(particles.begin(), particles.end(), [](auto const& particle) { return particle.parent >= 0; });
      UFW_ASSERT(n_daughters == static_cast<std::size_t>(n_with_parent),
                 "ixn[{}]: {} daughters listed but {} particles have a parent", i, n_daughters, n_with_parent);
    }

    // 3. Every tracker object belongs to exactly one reco particle
    UFW_ASSERT(std::all_of(track_users.begin(), track_users.end(), [](int n) { return n == 1; }),
               "some tracker tracks belong to no reco particle, or to more than one");
    UFW_ASSERT(std::all_of(shower_users.begin(), shower_users.end(), [](int n) { return n == 1; }),
               "some tracker showers belong to no reco particle, or to more than one");

    UFW_INFO("fast_reco_test: verified {} interaction(s)", common.ixn.sandreco.size());
  }

  fast_reco_test::~fast_reco_test() {
    UFW_INFO("fast_reco_test: the gate let {} track-/shower-like particle(s) through, rejected {}", m_gated_in,
             m_gated_out);
    UFW_ASSERT(m_gated_in > 0, "no particle passed the gate over the whole job: gate always false, or wrong input?");
    UFW_ASSERT(m_gated_out > 0, "no particle was rejected by the gate over the whole job: gate inactive?");
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::fast_reco_test)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::fast_reco_test)
