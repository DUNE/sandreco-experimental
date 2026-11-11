#include <caf/caf_wrapper.hpp>
#include <common/sand.h>

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
#include <numeric>
#include <vector>

namespace sand::test {

  /// Verifies the output of the fast reco chain (GRAIN, tracker and ECAL recos, matching, vertexing, pid, neutrino):
  /// 1. counters, the index alignment of common.ixn and nd.sand.ixn, and the interactions: one per neutrino with
  ///    something visible, never empty, never two for the same neutrino, at the neutrino's vertex;
  /// 2. the gate, over every neutrino and per subdetector: a true track-/shower-like particle has an object in GRAIN
  ///    (in the tracker) if and only if it left at least the GRAIN (tracker) `min_energy_deposit` there; any true
  ///    particle has an ECAL cluster iff it is a cluster root (born outside the ECAL, or a primary) and its ECAL
  ///    group (its hits and those of its descendants born inside the ECAL) has at least the ECAL
  ///    `min_energy_deposit`. All recomputed from sand::edep_reader and the truth without sharing code with the
  ///    recos (the ECAL groups are built bottom-up through parentID, the reco goes top-down through edep-sim's
  ///    children). A particle has a reco particle iff it has an object; a neutrino has a reco interaction iff at
  ///    least one of its particles is seen;
  /// 3. the links: particle j <-> trkmatch[j] <-> its objects (one per subdetector at most), with the same truth,
  ///    every object used once;
  /// 4. the objects: position, time, length, Evis and enddir from the hits in their subdetector, dir and E at the
  ///    entry point in it; ECAL clusters from the hits of their group, direction at the root's first ECAL point;
  /// 5. the parentage: the parent of a reco particle is its nearest visible true ancestor, and daughters mirror it;
  /// 6. part_energy_sum and part_mom_sum: sums over the visible primary particles of the interaction.
  struct fast_reco_test : public ufw::process {
    fast_reco_test();
    ~fast_reco_test() override;
    void configure(ufw::config const& cfg) override;
    void run() override;

   private:
    double m_tracker_min_energy_deposit{}; ///< same `min_energy_deposit` [MeV] as sand::fast::tracker::reco
    double m_grain_min_energy_deposit{};   ///< same `min_energy_deposit` [MeV] as sand::fast::grain::reco
    double m_ecal_min_energy_deposit{};    ///< same `min_energy_deposit` [MeV] as sand::fast::ecal::reco
    double m_b_field{};                    ///< same `b_field_magnitude` [T] as the tracker and GRAIN reco
    std::size_t m_seen{};                  ///< true particles with a reco particle
    std::size_t m_unseen{};                ///< true particles without one
    std::size_t m_ecal_and_other{};        ///< reco particles with an ECAL cluster and an object elsewhere
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

    /// The subdetectors with a fast reco
    enum class region { grain, tracker, ecal };

    char const* name_of(region r) {
      switch (r) {
      case region::grain:
        return "GRAIN";
      case region::tracker:
        return "tracker";
      default:
        return "ECAL";
      }
    }

    bool is_in(sand::subdetector_t det, region r) {
      switch (r) {
      case region::grain:
        return det == sand::subdetector_t::GRAIN;
      case region::tracker:
        return det == sand::subdetector_t::STT || det == sand::subdetector_t::DRIFT;
      default:
        return det == sand::subdetector_t::ECAL;
      }
    }

    /// The hits of a trajectory in a region, found without sorting: the earliest and the latest by start time (on
    /// equal times, the first and the last met), the summed length and the deposited energy.
    struct region_hits {
      EDEPHit const* first{};
      EDEPHit const* last{};
      double length_mm{};
      double energy{}; ///< [MeV]
    };

    region_hits find_hits(EDEPTrajectory const& trj, region r) {
      region_hits th;
      for (auto const& [det, hits] : trj.GetHitMap()) {
        if (!is_in(det, r)) {
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

    /// The earliest trajectory point in a region, null if edep-sim stored none there
    EDEPTrajectoryPoint const* find_entry_point(EDEPTrajectory const& trj, region r) {
      EDEPTrajectoryPoint const* entry{};
      for (auto const& [det, points] : trj.GetTrajectoryPoints()) {
        if (!is_in(det, r)) {
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

    /// As close, plus an absolute 1e-3 for the values that can be near 0 (coordinates [cm], spreads [cm]).
    bool near(double a, double b) { return std::abs(a - b) <= 1e-3 + 1e-5 * std::max(std::abs(a), std::abs(b)); }

    bool near(::caf::SRVector3D const& reco, double x, double y, double z) {
      return near(reco.x, x) && near(reco.y, y) && near(reco.z, z);
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

    /// A track in region `r`: position, time, length, Evis and enddir from the hits there; dir, E and R at the entry
    /// point there (E = R = NaN and dir from the first hit without one)
    void check_track(::caf::SRTrack const& track, EDEPTrajectory const& trj, ::caf::SRTrueParticle const& true_part,
                     region r, double b_field, std::size_t i, std::size_t j) {
      auto const* in  = name_of(r);
      auto const hits = find_hits(trj, r);
      UFW_ASSERT(hits.first != nullptr, "ixn[{}].part[{}]: {} track without hits there", i, j, in);
      UFW_ASSERT(same_position_cm(track.start, hits.first->GetStart()),
                 "ixn[{}].part[{}]: {} track start isn't the start of its first hit", i, j, in);
      UFW_ASSERT(same_position_cm(track.end, hits.last->GetStop()),
                 "ixn[{}].part[{}]: {} track end isn't the stop of its last hit", i, j, in);
      UFW_ASSERT(track.time == hits.first->GetStart().T(),
                 "ixn[{}].part[{}]: {} track time isn't the time of its first hit", i, j, in);
      UFW_ASSERT(close(track.len_cm, hits.length_mm * 0.1),
                 "ixn[{}].part[{}]: {} track length {} cm isn't the summed length of its hits {} cm", i, j, in,
                 track.len_cm, hits.length_mm * 0.1);
      UFW_ASSERT(close(track.Evis, hits.energy), "ixn[{}].part[{}]: {} track Evis {} MeV isn't the deposit {} MeV", i,
                 j, in, track.Evis, hits.energy);
      UFW_ASSERT(same_direction(track.enddir, *hits.last),
                 "ixn[{}].part[{}]: {} track enddir isn't the direction of its last hit", i, j, in);

      auto const* entry = find_entry_point(trj, r);
      if (entry == nullptr) {
        UFW_ASSERT(std::isnan(track.E), "ixn[{}].part[{}]: {} track without entry point, but E = {} MeV", i, j, in,
                   track.E);
        UFW_ASSERT(std::isnan(track.R), "ixn[{}].part[{}]: {} track without entry point, but R = {} cm", i, j, in,
                   track.R);
        UFW_ASSERT(same_direction(track.dir, *hits.first),
                   "ixn[{}].part[{}]: {} track without entry point, but dir isn't the first hit's", i, j, in);
        return;
      }
      auto const* pdg_info = TDatabasePDG::Instance()->GetParticle(true_part.pdg);
      double const mass    = pdg_info != nullptr ? pdg_info->Mass() * 1000. : 0.; // GeV -> MeV
      auto const& p_in     = entry->GetMomentum();                                // MeV
      UFW_ASSERT(close(track.E, std::sqrt(p_in.Mag2() + mass * mass)),
                 "ixn[{}].part[{}]: {} track E {} MeV isn't the energy at the entry point", i, j, in, track.E);
      // R [cm] = p_T [MeV] / (3 B [T]), p_T transverse to the field (along x)
      double const radius = std::hypot(p_in.y(), p_in.z()) / (3. * b_field);
      UFW_ASSERT(close(track.R, radius),
                 "ixn[{}].part[{}]: {} track R {} cm isn't the curvature radius at the entry point {} cm", i, j, in,
                 track.R, radius);
      UFW_ASSERT(track.E <= true_part.p.E * 1000. * (1. + 1e-5),
                 "ixn[{}].part[{}]: {} track E {} MeV is more than the true energy at the vertex {} MeV", i, j, in,
                 track.E, true_part.p.E * 1000.);
      if (!is_stopped(p_in)) {
        UFW_ASSERT(same_direction(track.dir, p_in),
                   "ixn[{}].part[{}]: {} track dir isn't the momentum at the entry point", i, j, in);
      }
    }

    /// A shower in region `r`: start, time and Evis from the hits there; direction at the entry point there, else
    /// from the first hit
    void check_shower(::caf::SRShower const& shower, EDEPTrajectory const& trj, region r, std::size_t i,
                      std::size_t j) {
      auto const* in  = name_of(r);
      auto const hits = find_hits(trj, r);
      UFW_ASSERT(hits.first != nullptr, "ixn[{}].part[{}]: {} shower without hits there", i, j, in);
      UFW_ASSERT(same_position_cm(shower.start, hits.first->GetStart()),
                 "ixn[{}].part[{}]: {} shower start isn't the start of its first hit", i, j, in);
      UFW_ASSERT(shower.time == hits.first->GetStart().T(),
                 "ixn[{}].part[{}]: {} shower time isn't the time of its first hit", i, j, in);
      UFW_ASSERT(close(shower.Evis, hits.energy), "ixn[{}].part[{}]: {} shower Evis {} MeV isn't the deposit {} MeV", i,
                 j, in, shower.Evis, hits.energy);

      auto const* entry = find_entry_point(trj, r);
      if (entry == nullptr) {
        UFW_ASSERT(same_direction(shower.direction, *hits.first),
                   "ixn[{}].part[{}]: {} shower without entry point, but direction isn't the first hit's", i, j, in);
      } else if (!is_stopped(entry->GetMomentum())) {
        UFW_ASSERT(same_direction(shower.direction, entry->GetMomentum()),
                   "ixn[{}].part[{}]: {} shower direction isn't the momentum at the entry point", i, j, in);
      }
    }

    /// The ECAL hits of a cluster root and of its descendants born inside the ECAL
    struct ecal_group {
      std::vector<EDEPHit const*> hits;
      double energy{}; ///< [MeV]
    };

    /// An ECAL cluster from the hits of its group: E, energy-weighted position and time, start of the earliest hit,
    /// spread with the legacy neff correction, num_cells not computed; direction from the momentum of the root at its
    /// first ECAL point
    void check_cluster(::caf::SRECALCluster const& cluster, ecal_group const& group, EDEPTrajectory const& root_trj,
                       std::size_t i, std::size_t j) {
      UFW_ASSERT(!group.hits.empty(), "ixn[{}].part[{}]: ECAL cluster for a particle that isn't a root with hits", i,
                 j);
      UFW_ASSERT(close(cluster.E, group.energy), "ixn[{}].part[{}]: ECAL cluster E {} MeV isn't the deposit {} MeV", i,
                 j, cluster.E, group.energy);

      double e{}, e2{}, t{}, earliest{};
      double sum[3]{}, sum_sq[3]{};
      for (auto const* hit : group.hits) {
        double const de = hit->GetEnergyDeposit();
        auto const mid  = 0.5 * (hit->GetStart() + hit->GetStop());
        double const c[3]{mid.X(), mid.Y(), mid.Z()};
        e += de;
        e2 += de * de;
        t += de * mid.T();
        for (int k{}; k != 3; ++k) {
          sum[k] += de * c[k];
          sum_sq[k] += de * c[k] * c[k];
        }
        earliest = (hit == group.hits.front()) ? hit->GetStart().T() : std::min(earliest, hit->GetStart().T());
      }
      double const neff = e * e / e2;
      double mean[3]{}, spread[3]{};
      for (int k{}; k != 3; ++k) {
        mean[k]          = sum[k] / e;
        double const var = sum_sq[k] / e - mean[k] * mean[k];
        spread[k] = (group.hits.size() < 2 || neff <= 1. || var <= 0.) ? 0. : std::sqrt(neff / (neff - 1.) * var);
      }

      UFW_ASSERT(near(cluster.position, mean[0] * 0.1, mean[1] * 0.1, mean[2] * 0.1),
                 "ixn[{}].part[{}]: ECAL cluster position isn't the energy-weighted barycenter of its hits", i, j);
      UFW_ASSERT(near(cluster.time, t / e), "ixn[{}].part[{}]: ECAL cluster time {} ns isn't the energy-weighted {} ns",
                 i, j, cluster.time, t / e);
      UFW_ASSERT(near(cluster.var_position, spread[0] * 0.1, spread[1] * 0.1, spread[2] * 0.1),
                 "ixn[{}].part[{}]: ECAL cluster var_position isn't the spread of its hits", i, j);
      UFW_ASSERT(std::any_of(group.hits.begin(), group.hits.end(),
                             [&](EDEPHit const* hit) {
                               return hit->GetStart().T() == earliest
                                   && same_position_cm(cluster.start, hit->GetStart());
                             }),
                 "ixn[{}].part[{}]: ECAL cluster start isn't the start of its earliest hit", i, j);
      UFW_ASSERT(cluster.num_cells == -1, "ixn[{}].part[{}]: ECAL cluster num_cells {}, expected -1 (not computed)", i,
                 j, cluster.num_cells);

      auto const* entry = find_entry_point(root_trj, region::ecal);
      if (entry == nullptr) {
        UFW_ASSERT(cluster.direction.x == 0.f && cluster.direction.y == 0.f && cluster.direction.z == 0.f,
                   "ixn[{}].part[{}]: ECAL cluster without an ECAL point of its root, but direction isn't (0, 0, 0)", i,
                   j);
      } else if (!is_stopped(entry->GetMomentum())) {
        UFW_ASSERT(same_direction(cluster.direction, entry->GetMomentum()),
                   "ixn[{}].part[{}]: ECAL cluster direction isn't the momentum of its root at its first ECAL point", i,
                   j);
      }
    }

    /// Region and kind of a constituent's collection
    struct collection_info {
      region where;
      bool is_track;
    };

    collection_info info_of(::caf::SRRecoBaseID::SRRecoBaseCollectionType type) {
      switch (type) {
      case ::caf::SRRecoBaseID::kSANDGRAINTrack:
        return {region::grain, true};
      case ::caf::SRRecoBaseID::kSANDGRAINShower:
        return {region::grain, false};
      case ::caf::SRRecoBaseID::kSANDTrackerTrack:
        return {region::tracker, true};
      case ::caf::SRRecoBaseID::kSANDTrackerShower:
        return {region::tracker, false};
      case ::caf::SRRecoBaseID::kSANDECalCluster:
        return {region::ecal, false};
      default:
        UFW_ERROR("unexpected constituent type {}", static_cast<int>(type));
      }
    }
  } // namespace

  void fast_reco_test::configure(ufw::config const& cfg) {
    process::configure(cfg);
    m_tracker_min_energy_deposit = cfg.value("tracker_min_energy_deposit", 0.);
    m_grain_min_energy_deposit   = cfg.value("grain_min_energy_deposit", 0.);
    m_ecal_min_energy_deposit    = cfg.value("ecal_min_energy_deposit", 0.);
    m_b_field                    = cfg.value("b_field_magnitude", 0.6);
    UFW_ASSERT(m_b_field > 0., "fast_reco_test: b_field_magnitude must be > 0, got {}", m_b_field);
  }

  void fast_reco_test::run() {
    auto const& truth   = get<sand::caf::truth_branch_wrapper>("in_truth");
    auto const& common  = get<sand::caf::common_reco_branch_wrapper>("in_common");
    auto const& nd      = get<sand::caf::nd_reco_branch_wrapper>("in_nd");
    auto const& edep    = instance<sand::edep_reader>();
    auto const& id_maps = instance<sand::spill_maps>();
    auto const& grain   = nd.sand.grain;
    auto const& tracker = nd.sand.tracker;
    auto const& ecal    = nd.sand.ecal;

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
    UFW_ASSERT(grain.tracks.size() == grain.ntracks, "grain.ntracks doesn't match its size");
    UFW_ASSERT(grain.showers.size() == grain.nshowers, "grain.nshowers doesn't match its size");
    UFW_ASSERT(tracker.tracks.size() == tracker.ntracks, "tracker.ntracks doesn't match its size");
    UFW_ASSERT(tracker.showers.size() == tracker.nshowers, "tracker.nshowers doesn't match its size");
    UFW_ASSERT(ecal.clusters.size() == ecal.nclusters, "ecal.nclusters doesn't match its size");
    UFW_ASSERT(common.ixn.sandreco.size() == nd.sand.ixn.size(),
               "common.ixn.sandreco ({}) and nd.sand.ixn ({}) are not index-aligned", common.ixn.sandreco.size(),
               nd.sand.ixn.size());

    // How many particles point to each object: exactly one at the end
    std::vector<int> grain_track_users(grain.tracks.size());
    std::vector<int> grain_shower_users(grain.showers.size());
    std::vector<int> tracker_track_users(tracker.tracks.size());
    std::vector<int> tracker_shower_users(tracker.showers.size());
    std::vector<int> ecal_cluster_users(ecal.clusters.size());

    // Which true particles were seen, for every neutrino: true particle -> reco particle j, -1 if invisible
    std::vector<std::vector<int>> prim_to_j(truth.nu.size());
    std::vector<std::vector<int>> sec_to_j(truth.nu.size());
    for (std::size_t t{}; t != truth.nu.size(); ++t) {
      prim_to_j[t].assign(truth.nu[t].prim.size(), -1);
      sec_to_j[t].assign(truth.nu[t].sec.size(), -1);
    }
    auto const reco_index = [&](::caf::TrueParticleID const& id) -> int& {
      auto& tables = (id.type == ::caf::TrueParticleID::kPrimary) ? prim_to_j : sec_to_j;
      return tables.at(static_cast<std::size_t>(id.ixn)).at(static_cast<std::size_t>(id.part));
    };

    // The ECAL groups, bottom-up from the truth: every true particle with ECAL hits gives them to its cluster root,
    // found by climbing parentID while the particle is a secondary born inside the ECAL. "Born inside" = the
    // particle's generation point (time and start_pos from the truth) is one of its ECAL trajectory points.
    std::vector<std::vector<ecal_group>> prim_group(truth.nu.size());
    std::vector<std::vector<ecal_group>> sec_group(truth.nu.size());
    std::vector<std::vector<char>> prim_born_in_ecal(truth.nu.size());
    std::vector<std::vector<char>> sec_born_in_ecal(truth.nu.size());
    auto const group_of = [&](::caf::TrueParticleID const& id) -> ecal_group& {
      auto& tables = (id.type == ::caf::TrueParticleID::kPrimary) ? prim_group : sec_group;
      return tables.at(static_cast<std::size_t>(id.ixn)).at(static_cast<std::size_t>(id.part));
    };
    auto const born_in_ecal = [&](::caf::TrueParticleID const& id) -> bool {
      auto const& tables = (id.type == ::caf::TrueParticleID::kPrimary) ? prim_born_in_ecal : sec_born_in_ecal;
      return tables.at(static_cast<std::size_t>(id.ixn)).at(static_cast<std::size_t>(id.part)) != 0;
    };
    for (std::size_t t{}; t != truth.nu.size(); ++t) {
      for (auto const type : {::caf::TrueParticleID::kPrimary, ::caf::TrueParticleID::kSecondary}) {
        bool const prim = type == ::caf::TrueParticleID::kPrimary;
        auto const n    = prim ? truth.nu[t].prim.size() : truth.nu[t].sec.size();
        (prim ? prim_group : sec_group)[t].resize(n);
        auto& born = (prim ? prim_born_in_ecal : sec_born_in_ecal)[t];
        born.resize(n);
        for (std::size_t k{}; k != n; ++k) {
          ::caf::TrueParticleID const id{static_cast<int>(t), type, static_cast<int>(k)};
          auto const& true_part = resolve_true(truth, id);
          auto const& points    = trajectory(id).GetTrajectoryPoints();
          auto const in_ecal    = points.find(sand::subdetector_t::ECAL);
          born[k]               = in_ecal != points.end()
                 && std::any_of(in_ecal->second.begin(), in_ecal->second.end(), [&](EDEPTrajectoryPoint const& point) {
                      return point.GetPosition().T() == true_part.time
                          && same_position_cm(true_part.start_pos, point.GetPosition());
                    });
        }
      }
    }
    for (std::size_t t{}; t != truth.nu.size(); ++t) {
      for (auto const type : {::caf::TrueParticleID::kPrimary, ::caf::TrueParticleID::kSecondary}) {
        auto const n = (type == ::caf::TrueParticleID::kPrimary) ? truth.nu[t].prim.size() : truth.nu[t].sec.size();
        for (std::size_t k{}; k != n; ++k) {
          ::caf::TrueParticleID const id{static_cast<int>(t), type, static_cast<int>(k)};
          auto const& hit_map  = trajectory(id).GetHitMap();
          auto const ecal_hits = hit_map.find(sand::subdetector_t::ECAL);
          if (ecal_hits == hit_map.end() || ecal_hits->second.empty()) {
            continue;
          }
          auto root = id;
          while (root.type == ::caf::TrueParticleID::kSecondary && born_in_ecal(root)) {
            root = resolve_true(truth, root).parentID;
            UFW_ASSERT(is_particle(root), "nu[{}]: a secondary born inside the ECAL without a saved parent", t);
          }
          auto& group = group_of(root);
          for (auto const& hit : ecal_hits->second) {
            group.hits.push_back(&hit);
            group.energy += hit.GetEnergyDeposit();
          }
        }
      }
    }

    // Which neutrino every reco interaction comes from: at most one interaction per neutrino
    std::vector<int> reco_of_true(truth.nu.size(), -1);

    for (std::size_t i{}; i != common.ixn.sandreco.size(); ++i) {
      auto const& reco_ixn  = common.ixn.sandreco[i];
      auto const& particles = reco_ixn.part.sandreco;
      auto const& sand_ixn  = nd.sand.ixn[i];
      auto const& trkmatch  = sand_ixn.trkmatch;
      auto const i_int      = static_cast<int>(i);

      UFW_ASSERT(particles.size() == static_cast<std::size_t>(reco_ixn.part.nsandreco),
                 "ixn[{}]: part.nsandreco doesn't match its size", i);
      UFW_ASSERT(trkmatch.size() == sand_ixn.ntrkmatch, "ixn[{}]: ntrkmatch doesn't match its size", i);
      UFW_ASSERT(particles.size() == trkmatch.size(), "ixn[{}]: {} reco particles but {} assns", i, particles.size(),
                 trkmatch.size());

      // Vertexing: an interaction exists only for a neutrino with something visible, once, at its true vertex
      UFW_ASSERT(!particles.empty(), "ixn[{}]: an interaction without visible particles", i);
      UFW_ASSERT(reco_ixn.truth.size() == 1 && reco_ixn.truth[0] < truth.nu.size(),
                 "ixn[{}]: expected one valid truth match", i);
      auto const t = reco_ixn.truth[0];
      UFW_ASSERT(reco_of_true[t] == -1, "ixn[{}]: neutrino {} already has interaction {}", i, t, reco_of_true[t]);
      reco_of_true[t] = i_int;
      UFW_ASSERT(reco_ixn.id == truth.nu[t].id && reco_ixn.vtx.x == truth.nu[t].vtx.x
                     && reco_ixn.vtx.y == truth.nu[t].vtx.y && reco_ixn.vtx.z == truth.nu[t].vtx.z
                     && reco_ixn.vtx_time == truth.nu[t].time,
                 "ixn[{}]: id, vertex or vertex time isn't the one of neutrino {}", i, t);

      for (std::size_t j{}; j != particles.size(); ++j) {
        auto const& particle = particles[j];
        auto const& assn     = trkmatch[j];
        auto const j_int     = static_cast<int>(j);

        // 3. Links: particle j <-> trkmatch[j] <-> its object, all with the same truth
        UFW_ASSERT(particle.truth.size() == 1 && particle.truthOverlap.size() == 1,
                   "ixn[{}].part[{}]: expected exactly one truth match", i, j);
        auto const& id = particle.truth[0];
        UFW_ASSERT(same_particle(id, assn.trk.truth.at(0)), "ixn[{}].part[{}]: truth differs from its assn's", i, j);
        UFW_ASSERT(static_cast<std::size_t>(id.ixn) == t, "ixn[{}].part[{}]: belongs to neutrino {}, not {}", i, j,
                   id.ixn, t);
        UFW_ASSERT(reco_index(id) == -1, "ixn[{}].part[{}]: true particle already has a reco particle", i, j);
        reco_index(id) = j_int;

        auto const& true_part = resolve_true(truth, id);
        UFW_ASSERT(particle.pdg == true_part.pdg, "ixn[{}].part[{}]: pdg {} isn't the true {}", i, j, particle.pdg,
                   true_part.pdg);
        UFW_ASSERT(particle.E == true_part.p.E, "ixn[{}].part[{}]: E {} isn't the true {} GeV (unit mismatch?)", i, j,
                   particle.E, true_part.p.E);
        UFW_ASSERT(particle.primary == (id.type == ::caf::TrueParticleID::kPrimary),
                   "ixn[{}].part[{}]: primary flag disagrees with the truth", i, j);

        UFW_ASSERT(particle.recoobj.type == ::caf::SRRecoBaseID::kSANDAssn && particle.recoobj.ixn == i_int
                       && particle.recoobj.irecoobj == j_int,
                   "ixn[{}].part[{}]: recoobj isn't trkmatch[{}] of this interaction", i, j, j);
        UFW_ASSERT(same_reco_particle(assn.part, i_int, j_int), "ixn[{}].part[{}]: assn.part doesn't point back", i, j);
        UFW_ASSERT(!assn.constituents.empty() && assn.constituents.size() <= 3,
                   "ixn[{}].part[{}]: {} constituents, expected 1 to 3 (GRAIN, tracker, ECAL)", i, j,
                   assn.constituents.size());
        UFW_ASSERT(particle.origRecoObjType
                       == (info_of(assn.constituents.front().type).is_track ? ::caf::RecoObjType::kTrack
                                                                            : ::caf::RecoObjType::kShower),
                   "ixn[{}].part[{}]: origRecoObjType isn't the kind of the first constituent", i, j);

        auto const& trj = trajectory(id);
        bool in_region[3]{};
        for (auto const& constituent : assn.constituents) {
          auto const [where, is_track] = info_of(constituent.type);
          auto const idx               = static_cast<std::size_t>(constituent.idx);
          auto& seen_here              = in_region[static_cast<int>(where)];
          UFW_ASSERT(!seen_here, "ixn[{}].part[{}]: two constituents in {}", i, j, name_of(where));
          seen_here = true;

          // 4. The objects, each from the hits in its own subdetector
          if (where == region::ecal) {
            UFW_ASSERT(idx < ecal.clusters.size(), "ixn[{}].part[{}]: ECAL cluster index out of range", i, j);
            auto const& cluster = ecal.clusters[idx];
            UFW_ASSERT(same_reco_particle(cluster.part, i_int, j_int) && same_particle(cluster.truth.at(0), id),
                       "ixn[{}].part[{}]: its ECAL cluster doesn't point back, or has another truth", i, j);
            ++ecal_cluster_users[idx];
            check_cluster(cluster, group_of(id), trj, i, j);
          } else if (is_track) {
            auto const& tracks = where == region::grain ? grain.tracks : tracker.tracks;
            auto& users        = where == region::grain ? grain_track_users : tracker_track_users;
            UFW_ASSERT(idx < tracks.size(), "ixn[{}].part[{}]: {} track index out of range", i, j, name_of(where));
            auto const& track = tracks[idx];
            UFW_ASSERT(is_track_like(particle.pdg), "ixn[{}].part[{}]: pdg {} got a track", i, j, particle.pdg);
            UFW_ASSERT(same_reco_particle(track.part, i_int, j_int) && same_particle(track.truth.at(0), id),
                       "ixn[{}].part[{}]: its {} track doesn't point back, or has another truth", i, j, name_of(where));
            ++users[idx];
            check_track(track, trj, true_part, where, m_b_field, i, j);
          } else {
            auto const& showers = where == region::grain ? grain.showers : tracker.showers;
            auto& users         = where == region::grain ? grain_shower_users : tracker_shower_users;
            UFW_ASSERT(idx < showers.size(), "ixn[{}].part[{}]: {} shower index out of range", i, j, name_of(where));
            auto const& shower = showers[idx];
            UFW_ASSERT(is_shower_like(particle.pdg), "ixn[{}].part[{}]: pdg {} got a shower", i, j, particle.pdg);
            UFW_ASSERT(same_reco_particle(shower.part, i_int, j_int) && same_particle(shower.truth.at(0), id),
                       "ixn[{}].part[{}]: its {} shower doesn't point back, or has another truth", i, j,
                       name_of(where));
            ++users[idx];
            check_shower(shower, trj, where, i, j);
          }
        }
        if (in_region[static_cast<int>(region::ecal)] && assn.constituents.size() > 1) {
          ++m_ecal_and_other;
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

      // 6. part_energy_sum and part_mom_sum: the four-momentum summed over the visible primaries
      auto const sum = std::accumulate(
          particles.begin(), particles.end(), sand::mom_4d{},
          [](sand::mom_4d const& acc, ::caf::SRRecoParticle const& particle) {
            return particle.primary ? acc + sand::mom_4d{particle.p.x, particle.p.y, particle.p.z, particle.E} : acc;
          });
      UFW_ASSERT(close(reco_ixn.Enu.part_energy_sum, sum.E()),
                 "ixn[{}]: part_energy_sum {} GeV isn't the energy of the visible primaries {} GeV", i,
                 reco_ixn.Enu.part_energy_sum, sum.E());
      auto const& mom_dir  = reco_ixn.dir.part_mom_sum;
      bool const no_mom    = sum.Px() == 0. && sum.Py() == 0. && sum.Pz() == 0.;
      bool const right_dir = no_mom ? (mom_dir.x == 0.f && mom_dir.y == 0.f && mom_dir.z == 0.f)
                                    : same_direction(mom_dir, sum.Px(), sum.Py(), sum.Pz());
      UFW_ASSERT(right_dir, "ixn[{}]: part_mom_sum isn't the direction of the momentum of the visible primaries", i);
    }

    // 2. Gate, over every neutrino (also those with no reco interaction) and per subdetector: a track-/shower-like
    //    true particle has an object in GRAIN or the tracker iff it left enough energy there; any particle has an
    //    ECAL cluster iff its ECAL group (empty unless it is a root) has enough energy; it is seen iff it has an
    //    object anywhere. A neutrino with nothing seen must have no interaction.
    for (std::size_t t{}; t != truth.nu.size(); ++t) {
      bool any_seen{};
      for (auto const type : {::caf::TrueParticleID::kPrimary, ::caf::TrueParticleID::kSecondary}) {
        auto const n = (type == ::caf::TrueParticleID::kPrimary) ? truth.nu[t].prim.size() : truth.nu[t].sec.size();
        for (std::size_t k{}; k != n; ++k) {
          ::caf::TrueParticleID const id{static_cast<int>(t), type, static_cast<int>(k)};
          auto const& true_part = resolve_true(truth, id);
          bool const candidate  = is_track_like(true_part.pdg) || is_shower_like(true_part.pdg);
          auto const& trj       = trajectory(id);
          int const j           = reco_index(id);

          for (auto const r : {region::grain, region::tracker, region::ecal}) {
            bool expected{};
            double deposit{}; // [MeV], for the message
            if (r == region::ecal) {
              auto const& group = group_of(id);
              deposit           = group.energy;
              expected          = !group.hits.empty() && group.energy > 0. && group.energy >= m_ecal_min_energy_deposit;
            } else {
              auto const hits    = find_hits(trj, r);
              auto const min_dep = r == region::grain ? m_grain_min_energy_deposit : m_tracker_min_energy_deposit;
              deposit            = hits.energy;
              expected           = candidate && hits.first != nullptr && hits.energy >= min_dep;
            }
            bool has_object{};
            if (j >= 0) {
              auto const& sand_ixn     = nd.sand.ixn[static_cast<std::size_t>(reco_of_true[t])];
              auto const& constituents = sand_ixn.trkmatch[static_cast<std::size_t>(j)].constituents;
              has_object               = std::any_of(constituents.begin(), constituents.end(),
                                                     [&](auto const& c) { return info_of(c.type).where == r; });
            }
            UFW_ASSERT(has_object == expected,
                       "nu[{}]: true particle pdg {} (G4ID {}, {} MeV in {}) {} have an object there", t, true_part.pdg,
                       true_part.G4ID, deposit, name_of(r), expected ? "should" : "should not");
          }

          bool const seen = j >= 0;
          ++(seen ? m_seen : m_unseen);
          any_seen = any_seen || seen;
        }
      }
      UFW_ASSERT(any_seen == (reco_of_true[t] >= 0), "nu[{}]: {} visible particles but {} reco interaction", t,
                 any_seen ? "has" : "has no", reco_of_true[t] >= 0 ? "a" : "no");
    }

    // 3. Every object belongs to exactly one reco particle
    auto const used_once = [](std::vector<int> const& users) {
      return std::all_of(users.begin(), users.end(), [](int n) { return n == 1; });
    };
    UFW_ASSERT(used_once(grain_track_users) && used_once(grain_shower_users),
               "some GRAIN objects belong to no reco particle, or to more than one");
    UFW_ASSERT(used_once(tracker_track_users) && used_once(tracker_shower_users),
               "some tracker objects belong to no reco particle, or to more than one");
    UFW_ASSERT(used_once(ecal_cluster_users), "some ECAL clusters belong to no reco particle, or to more than one");

    UFW_INFO("fast_reco_test: verified {} interaction(s)", common.ixn.sandreco.size());
  }

  fast_reco_test::~fast_reco_test() {
    UFW_INFO("fast_reco_test: {} true particle(s) seen, {} not; {} reco particle(s) with an ECAL cluster and an object "
             "in another subdetector",
             m_seen, m_unseen, m_ecal_and_other);
    UFW_ASSERT(m_seen > 0, "no particle passed the gate over the whole job: gate always false, or wrong input?");
    UFW_ASSERT(m_unseen > 0, "no particle was rejected by the gate over the whole job: gate inactive?");
    UFW_ASSERT(m_ecal_and_other > 0, "no particle seen in the ECAL and elsewhere over the whole job: matching broken?");
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::fast_reco_test)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::fast_reco_test)
