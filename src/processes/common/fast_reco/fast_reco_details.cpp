#include "fast_reco_details.hpp"

#include <duneanaobj/StandardRecord/SRDirectionBranch.h>
#include <duneanaobj/StandardRecord/SREnums.h>
#include <duneanaobj/StandardRecord/SRNeutrinoEnergyBranch.h>
#include <duneanaobj/StandardRecord/SRNeutrinoHypothesisBranch.h>
#include <duneanaobj/StandardRecord/SRRecoParticle.h>
#include <duneanaobj/StandardRecord/SRRecoParticlesBranch.h>
#include <duneanaobj/StandardRecord/SRShower.h>
#include <duneanaobj/StandardRecord/SRTrack.h>
#include <duneanaobj/StandardRecord/SRSAND.h>
#include <duneanaobj/StandardRecord/SRTrueInteraction.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>

#include <TDatabasePDG.h>
#include <TParticlePDG.h>

#include <cmath>
#include <cstddef>

namespace sand::common::reco_details {

  namespace {
    /// @return true for mu, pi+/-, K+/-, p.
    bool is_track_like(int pdg) {
      int const abs_pdg = std::abs(pdg);
      return abs_pdg == 13 || abs_pdg == 211 || abs_pdg == 321 || abs_pdg == 2212;
    }

    /// @return true for e+/-, gamma, pi0.
    bool is_shower_like(int pdg) {
      int const abs_pdg = std::abs(pdg);
      return abs_pdg == 11 || abs_pdg == 22 || abs_pdg == 111;
    }

    /// Unit vector along (px, py, pz), or the zero vector if the input is null.
    ::caf::SRVector3D normalize_to_direction(float px, float py, float pz) {
      float const mag = std::sqrt(px * px + py * py + pz * pz);
      return (mag > 0.f) ? ::caf::SRVector3D{px / mag, py / mag, pz / mag} : ::caf::SRVector3D{0.f, 0.f, 0.f};
    }

    /// One-hot encoding over the {0, 1, 2, N>=3} multiplicity buckets used by SRCVNScoreBranch.
    std::array<float, 4> count_bucket_one_hot(int count) {
      std::array bucket{0.f, 0.f, 0.f, 0.f};
      bucket[static_cast<std::size_t>(std::min(count, 3))] = 1.f;
      return bucket;
    }

    /// @return +1/-1/0 from TDatabasePDG's charge for `pdg`, or 0 if the PDG code is unknown.
    short charge_from_pdg(int pdg) {
      if (auto const* p = TDatabasePDG::Instance()->GetParticle(pdg)) {
        double const q = p->Charge() / 3.0;
        if (q > 0.5) {
          return 1;
        }
        if (q < -0.5) {
          return -1;
        }
      }
      return 0;
    }

    /// Euclidean distance between two points.
    float distance(::caf::SRVector3D const& a, ::caf::SRVector3D const& b) {
      float const dx = b.x - a.x;
      float const dy = b.y - a.y;
      float const dz = b.z - a.z;
      return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    /// Resolves a TrueParticleID (as produced by particle_slots_from_true()) back to the
    /// SRTrueParticle it refers to.
    ::caf::SRTrueParticle const& true_particle_from_id(::caf::SRTrueInteraction const& true_ixn,
                                                       ::caf::TrueParticleID const& id) {
      return (id.type == ::caf::TrueParticleID::kPrimary) ? true_ixn.prim[id.part] : true_ixn.sec[id.part];
    }

    /// Maps a TrueParticleID (typically SRTrueParticle::parentID) to its SRRecoParticle index in
    /// SRRecoParticlesBranch::sandreco, or -1 if it has no reco parent (kUnknown/prefsi).
    /// prim[i] always lands at part_idx i, sec[i] at part_idx n_prim + i, because
    /// particle_slots_from_true appends all prim before any sec, in the same order.
    int reco_idx_from_id(::caf::TrueParticleID const& id, int n_prim) {
      switch (id.type) {
      case ::caf::TrueParticleID::kPrimary:
        return id.part;
      case ::caf::TrueParticleID::kSecondary:
        return n_prim + id.part;
      default:
        return -1; // no parent, or prefsi (not modeled here)
      }
    }
  } // namespace

  ::caf::SRNeutrinoHypothesisBranch neutrino_hypothesis_from_true(::caf::SRTrueInteraction const& true_ixn) {
    ::caf::SRNeutrinoHypothesisBranch nuhyp{};
    auto& cvn = nuhyp.cvn;

    int const abs_pdg = std::abs(true_ixn.pdg);
    cvn.isnubar       = (true_ixn.pdg < 0) ? 1.f : 0.f;
    cvn.nc            = true_ixn.iscc ? 0.f : 1.f;
    cvn.nue           = (true_ixn.iscc && abs_pdg == 12) ? 1.f : 0.f;
    cvn.numu          = (true_ixn.iscc && abs_pdg == 14) ? 1.f : 0.f;
    cvn.nutau         = (true_ixn.iscc && abs_pdg == 16) ? 1.f : 0.f;

    auto const [p0, p1, p2, pN] = count_bucket_one_hot(true_ixn.nproton);
    cvn.protons0                = p0;
    cvn.protons1                = p1;
    cvn.protons2                = p2;
    cvn.protonsN                = pN;

    auto const [c0, c1, c2, cN] = count_bucket_one_hot(true_ixn.npip + true_ixn.npim);
    cvn.chgpi0                  = c0;
    cvn.chgpi1                  = c1;
    cvn.chgpi2                  = c2;
    cvn.chgpiN                  = cN;

    auto const [z0, z1, z2, zN] = count_bucket_one_hot(true_ixn.npi0);
    cvn.pizero0                 = z0;
    cvn.pizero1                 = z1;
    cvn.pizero2                 = z2;
    cvn.pizeroN                 = zN;

    auto const [n0, n1, n2, nN] = count_bucket_one_hot(true_ixn.nneutron);
    cvn.neutron0                = n0;
    cvn.neutron1                = n1;
    cvn.neutron2                = n2;
    cvn.neutronN                = nN;

    return nuhyp;
  }

  ::caf::SRDirectionBranch direction_from_true(::caf::SRTrueInteraction const& true_ixn) {
    ::caf::SRDirectionBranch dir{};

    auto const true_dir = normalize_to_direction(true_ixn.momentum.x, true_ixn.momentum.y, true_ixn.momentum.z);
    dir.calo            = true_dir;
    dir.heshw           = true_dir;
    dir.lngtrk          = true_dir;
    dir.part_mom_sum    = true_dir;

    return dir;
  }

  ::caf::SRNeutrinoEnergyBranch energy_from_true(::caf::SRTrueInteraction const& true_ixn) {
    ::caf::SRNeutrinoEnergyBranch enu{};

    enu.calo        = true_ixn.E;
    enu.lep_calo    = true_ixn.E;
    enu.mu_range    = true_ixn.E;
    enu.mu_mcs      = true_ixn.E;
    enu.mu_mcs_llhd = true_ixn.E;
    enu.e_calo      = true_ixn.E;

    enu.e_had  = true_ixn.E;
    enu.mu_had = true_ixn.E;

    enu.regcnn = true_ixn.E;

    enu.part_energy_sum = true_ixn.E;

    return enu;
  }

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

    if (is_track_like(true_part.pdg)) {
      reco_p.origRecoObjType = ::caf::RecoObjType::kTrack;
    } else if (is_shower_like(true_part.pdg)) {
      reco_p.origRecoObjType = ::caf::RecoObjType::kShower;
    }

    reco_p.truth.push_back(id);
    reco_p.truthOverlap.push_back(1.f);

    return reco_p;
  }

  ::caf::SRTrack track_from_true(::caf::SRTrueParticle const& true_part, ::caf::TrueParticleID const& id) {
    ::caf::SRTrack track{};

    track.start  = true_part.start_pos;
    track.end    = true_part.end_pos;
    track.dir    = normalize_to_direction(true_part.p.px, true_part.p.py, true_part.p.pz);
    track.enddir = track.dir;
    track.time   = true_part.time;
    track.E      = true_part.p.E;
    track.Evis   = track.E;
    track.len_cm = distance(track.start, track.end);
    track.charge = charge_from_pdg(true_part.pdg);
    track.qual   = 1.f;

    track.truth.push_back(id);
    track.truthOverlap.push_back(1.f);

    return track;
  }

  ::caf::SRShower shower_from_true(::caf::SRTrueParticle const& true_part, ::caf::TrueParticleID const& id) {
    ::caf::SRShower shower{};

    shower.start     = true_part.start_pos;
    shower.direction = normalize_to_direction(true_part.p.px, true_part.p.py, true_part.p.pz);
    shower.Evis      = true_part.p.E;

    shower.truth.push_back(id);
    shower.truthOverlap.push_back(1.f);

    return shower;
  }

  ParticleSlots particle_slots_from_true(::caf::SRTrueInteraction const& true_ixn, int ixn_idx) {
    ParticleSlots slots;
    int track_idx{};
    int shower_idx{};

    auto add = [&](::caf::SRTrueParticle const& true_part, ::caf::TrueParticleID const& id) {
      ParticleSlot slot{id, static_cast<int>(slots.size())};
      if (is_track_like(true_part.pdg)) {
        slot.track_idx = track_idx++;
      } else if (is_shower_like(true_part.pdg)) {
        slot.shower_idx = shower_idx++;
      }
      slots.push_back(slot);
    };

    auto add_all = [&](std::vector<::caf::SRTrueParticle> const& particles, ::caf::TrueParticleID::PartType type) {
      for (std::size_t i{}; i != particles.size(); ++i) {
        add(particles[i], {ixn_idx, type, static_cast<int>(i)});
      }
    };

    add_all(true_ixn.prim, ::caf::TrueParticleID::kPrimary);
    add_all(true_ixn.sec, ::caf::TrueParticleID::kSecondary);

    return slots;
  }

  ::caf::SRRecoParticlesBranch reco_particles_from_true(::caf::SRTrueInteraction const& true_ixn, int ixn_idx) {
    ::caf::SRRecoParticlesBranch part{};
    auto const n_prim = static_cast<int>(true_ixn.prim.size());

    for (auto const& slot : particle_slots_from_true(true_ixn, ixn_idx)) {
      auto const& true_part = true_particle_from_id(true_ixn, slot.id);
      auto reco_p           = reco_particle_from_true(true_part, slot.id);

      int const parent_idx = reco_idx_from_id(true_part.parentID, n_prim);
      if (parent_idx >= 0) {
        reco_p.parent = parent_idx;
        part.sandreco[parent_idx].daughters.push_back(static_cast<unsigned int>(slot.part_idx));
      }

      if (slot.track_idx >= 0) {
        reco_p.recoobj = {ixn_idx, ::caf::SRRecoBaseID::kSANDTrackerTrack, slot.track_idx};
      } else if (slot.shower_idx >= 0) {
        reco_p.recoobj = {ixn_idx, ::caf::SRRecoBaseID::kSANDTrackerShower, slot.shower_idx};
      }

      part.sandreco.push_back(std::move(reco_p));
      ++part.nsandreco;
    }
    return part;
  }

  ::caf::SRTracker sand_tracker_from_true(::caf::SRTrueInteraction const& true_ixn, int ixn_idx) {
    ::caf::SRTracker tracker{};

    for (auto const& slot : particle_slots_from_true(true_ixn, ixn_idx)) {
      auto const& true_part = true_particle_from_id(true_ixn, slot.id);

      if (slot.track_idx >= 0) {
        auto track = track_from_true(true_part, slot.id);
        track.part = {ixn_idx, ::caf::SRRecoParticleID::kSandreco, slot.part_idx};
        tracker.tracks.push_back(std::move(track));
        ++tracker.ntracks;
      } else if (slot.shower_idx >= 0) {
        auto shower = shower_from_true(true_part, slot.id);
        shower.part = {ixn_idx, ::caf::SRRecoParticleID::kSandreco, slot.part_idx};
        tracker.showers.push_back(std::move(shower));
        ++tracker.nshowers;
      }
    }
    return tracker;
  }

} // namespace sand::common::reco_details
