#include "reco.hpp"

#include <caf/caf_wrapper.hpp>
#include <edep_reader/edep_reader.hpp>
#include <spill_maps/spill_maps.hpp>

#include <ufw/factory.hpp>
#include <ufw/utils.hpp>

#include <cmath>
#include <cstddef>
#include <numeric>
#include <vector>

namespace sand::fast::ecal {

  namespace {

    /// True if the trajectory starts inside the ECAL. The edep_reader files the first point of the trajectory as the
    /// front of the first points of the subdetector where it starts, so the trajectory is born in the ECAL if the
    /// front of the ECAL first points is not later than the front of any other subdetector.
    bool born_inside_ecal(EDEPTrajectory const& trj) {
      auto const& ecal_first_points = trj.GetFirstPointsInDetector(subdetector_t::ECAL);
      if (ecal_first_points.empty()) {
        return false;
      }
      double const t_ecal = ecal_first_points.front().GetPosition().T();
      for (unsigned c = subdetector_t::kSandBegin; c != subdetector_t::kLoopableEnd; ++c) {
        auto const& first_points = trj.GetFirstPointsInDetector(static_cast<subdetector_t>(c));
        if (!first_points.empty() && first_points.front().GetPosition().T() < t_ecal) {
          return false;
        }
      }
      return true;
    }

    /// The first trajectory point of `trj` in the ECAL (where it enters, or is born if born inside), null if edep-sim
    /// stored none there. The edep_reader keeps the first points of every entry in step order, so the first entry is
    /// the front.
    EDEPTrajectoryPoint const* ecal_entry_point(EDEPTrajectory const& trj) {
      auto const& first_points = trj.GetFirstPointsInDetector(subdetector_t::ECAL);
      return first_points.empty() ? nullptr : &first_points.front();
    }

    ::caf::SRVector3D normalize_to_direction(double x, double y, double z) {
      auto const fx   = static_cast<float>(x);
      auto const fy   = static_cast<float>(y);
      auto const fz   = static_cast<float>(z);
      float const mag = std::sqrt(fx * fx + fy * fy + fz * fz);
      return (mag > 0.f) ? ::caf::SRVector3D{fx / mag, fy / mag, fz / mag} : ::caf::SRVector3D{0.f, 0.f, 0.f};
    }

    /// edep-sim positions are in mm, CAF positions in cm
    ::caf::SRVector3D position_cm(sand::vec_4d const& v) {
      return {static_cast<float>(v.X() * 0.1), static_cast<float>(v.Y() * 0.1), static_cast<float>(v.Z() * 0.1)};
    }

    double energy_deposit(std::vector<EDEPHit> const& hits) {
      return std::accumulate(hits.begin(), hits.end(), 0.,
                             [](double sum, EDEPHit const& hit) { return sum + hit.GetEnergyDeposit(); });
    }

    /// Appends the ECAL hits of `trj` and of its descendants born inside the ECAL: the deposits of the cluster of a
    /// particle entering the ECAL. A child born outside the ECAL is the root of a cluster of its own.
    void collect_ecal_hits(EDEPTrajectory const& trj, std::vector<EDEPHit>& hits) {
      if (auto const it = trj.GetHitMap().find(subdetector_t::ECAL); it != trj.GetHitMap().end()) {
        hits.insert(hits.end(), it->second.begin(), it->second.end());
      }
      for (auto const& child : trj.GetChildrenTrajectories()) {
        if (born_inside_ecal(child)) {
          collect_ecal_hits(child, hits);
        }
      }
    }

    /// E, position and time from the hits (energy-weighted, as the legacy ECAL clustering), start from the earliest
    /// hit, direction from the momentum of the entering particle at its first ECAL point ((0, 0, 0) without one).
    /// var_position is the energy-weighted spread of the hits with the legacy neff correction: the legacy weights the
    /// cells, here the hits, finer than a cell, so a narrow cluster comes out narrower than the detector would see it.
    /// num_cells is -1 (not computed: counting the cells needs sand::geoinfo).
    ::caf::SRECALCluster cluster_from(std::vector<EDEPHit> const& hits, EDEPTrajectoryPoint const* entry,
                                      ::caf::TrueParticleID const& id, int index) {
      double e{}, e2{};
      sand::vec_4d weighted{};
      double x2{}, y2{}, z2{}; // energy-weighted sums of the squared coordinates [MeV mm²]
      EDEPHit const* first{};
      for (auto const& hit : hits) {
        double const de = hit.GetEnergyDeposit();                 // MeV
        auto const mid  = 0.5 * (hit.GetStart() + hit.GetStop()); // mm, ns
        e += de;
        e2 += de * de;
        weighted += de * mid;
        x2 += de * mid.X() * mid.X();
        y2 += de * mid.Y() * mid.Y();
        z2 += de * mid.Z() * mid.Z();
        if (first == nullptr || hit.GetStart().T() < first->GetStart().T()) {
          first = &hit;
        }
      }
      auto const barycenter = weighted / e;

      // Legacy spread: sqrt(neff / (neff - 1) * (<x²> - <x>²)), neff = E² / ΣE² the effective number of hits;
      // 0 for a single hit (neff = 1) or a negative variance from rounding
      double const neff = e * e / e2;
      auto const spread = [&](double sum_sq, double mean) {
        double const var = sum_sq / e - mean * mean;
        return (hits.size() < 2 || neff <= 1. || var <= 0.) ? 0. : std::sqrt(neff / (neff - 1.) * var);
      };

      ::caf::SRECALCluster cluster{};
      cluster.id           = index;
      cluster.E            = static_cast<float>(e); // visible energy [MeV]
      cluster.position     = position_cm(barycenter);
      cluster.var_position = ::caf::SRVector3D{static_cast<float>(spread(x2, barycenter.X()) * 0.1), // mm -> cm
                                               static_cast<float>(spread(y2, barycenter.Y()) * 0.1),
                                               static_cast<float>(spread(z2, barycenter.Z()) * 0.1)};
      cluster.time         = static_cast<float>(barycenter.T());
      cluster.start        = position_cm(first->GetStart());
      cluster.num_cells    = -1;

      if (entry != nullptr) {
        auto const& p     = entry->GetMomentum();
        cluster.direction = normalize_to_direction(p.X(), p.Y(), p.Z());
      } else {
        cluster.direction = ::caf::SRVector3D{0.f, 0.f, 0.f};
      }

      cluster.truth.push_back(id);
      cluster.truthOverlap.push_back(1.f);
      return cluster;
    }

  } // namespace

  reco::reco()
    : process{{{"in_truth", "sand::caf::truth_branch_wrapper"}}, {{"out_ecal", "sand::caf::ecal_reco_wrapper"}}} {}

  void reco::configure(ufw::config const& cfg) {
    process::configure(cfg);
    m_min_energy_deposit = cfg.value("min_energy_deposit", 0.);

    UFW_ASSERT(m_min_energy_deposit >= 0., "fast::ecal::reco: min_energy_deposit must be >= 0, got {}",
               m_min_energy_deposit);
  }

  void reco::run() {
    auto const& edep    = instance<sand::edep_reader>();
    auto const& id_maps = instance<sand::spill_maps>();
    auto const& truth   = get<sand::caf::truth_branch_wrapper>("in_truth");
    auto& ecal          = set<sand::caf::ecal_reco_wrapper>("out_ecal");

    int n_below_threshold{}, n_no_entry_point{};

    for (std::size_t ixn{}; ixn != truth.nu.size(); ++ixn) {
      auto const& ids = id_maps.at(ixn);

      // Every true particle of the interaction: primaries, then secondaries
      for (auto const type : {::caf::TrueParticleID::kPrimary, ::caf::TrueParticleID::kSecondary}) {
        auto const& particles = (type == ::caf::TrueParticleID::kPrimary) ? truth.nu[ixn].prim : truth.nu[ixn].sec;
        for (std::size_t idx{}; idx != particles.size(); ++idx) {
          auto const& p = particles[idx];
          ::caf::TrueParticleID const id{static_cast<int>(ixn), type, static_cast<int>(idx)};

          auto const trj = edep.GetTrajectory(ids.to_spill_id(p.G4ID));
          if (trj == edep.end()) {
            UFW_ERROR("nu[{}]: true particle G4ID {} has no edep-sim trajectory", ixn, p.G4ID);
          }

          // A cluster root is a particle entering the ECAL, or a primary born inside it (an interaction in the
          // ECAL); a secondary born inside belongs to the cluster of its ancestor
          if (type == ::caf::TrueParticleID::kSecondary && born_inside_ecal(*trj)) {
            continue;
          }

          std::vector<EDEPHit> hits;
          collect_ecal_hits(*trj, hits);
          if (hits.empty()) {
            continue;
          }
          // energy > 0 also keeps the energy-weighted barycenter finite when min_energy_deposit is 0
          if (double const energy = energy_deposit(hits); energy <= 0. || energy < m_min_energy_deposit) {
            ++n_below_threshold;
            continue;
          }

          auto const* entry = ecal_entry_point(*trj);
          n_no_entry_point += entry == nullptr ? 1 : 0;
          ecal.clusters.push_back(cluster_from(hits, entry, id, static_cast<int>(ecal.clusters.size())));
        }
      }
    }
    ecal.nclusters = ecal.clusters.size();

    UFW_DEBUG("fast::ecal::reco: {} clusters; {} below min_energy_deposit ({} MeV); {} clusters without an ECAL point "
              "of the entering particle (direction unknown)",
              ecal.nclusters, n_below_threshold, m_min_energy_deposit, n_no_entry_point);
  }

} // namespace sand::fast::ecal

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::fast::ecal::reco);
