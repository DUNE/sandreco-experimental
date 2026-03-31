#include "caf_filler.hpp"

#include<random>

namespace sand {

  namespace {
    [[nodiscard]] ::caf::SRVector3D direction_from_momentum(const ::caf::SRTrueParticle& p) {
      return normalize_to_direction(p.p.px, p.p.py, p.p.pz);
    }

    [[nodiscard]] constexpr float gev_to_mev(float E) { return E * 1000.0f; }
  } // namespace

  namespace fake_reco_constants {
    constexpr float E_res    = 0.057f; // sigma = 5.7% sqrtE in gev
    constexpr float time_res = 0.054f; // sigma = 54/sqrtE
    constexpr float r_res    = 1.3f;   // position res, cm
    constexpr float p_res    = 0.04f;  // sigma = 4% p

  }

  ::caf::SRRecoParticle CAFFiller<::caf::SRRecoParticle>::from_true(const ::caf::SRTrueParticle& true_part,
                                                                    const ::caf::TrueParticleID& id) {
    ::caf::SRRecoParticle reco{};

    reco.primary  = true;
    reco.pdg      = true_part.pdg;
    reco.score    = 1.0f;
    reco.E        = true_part.p.E;
    reco.E_method = ::caf::PartEMethod::kCalorimetry;
    reco.p        = ::caf::SRVector3D{true_part.p.px, true_part.p.py, true_part.p.pz};
    reco.start    = true_part.start_pos;
    reco.end      = true_part.end_pos;

    if (is_track_like(true_part.pdg)) {
      reco.origRecoObjType = ::caf::RecoObjType::kTrack;
    } else if (is_shower_like(true_part.pdg)) {
      reco.origRecoObjType = ::caf::RecoObjType::kShower;
    }
    // else the particle is neutral cannot be reconstructed

    add_truth_match(reco, id);
    return reco;
  }

  ::caf::SRRecoParticle CAFFiller<::caf::SRRecoParticle>::from_true_with_smearing(const ::caf::SRTrueParticle& true_part,
                                                                       const ::caf::TrueParticleID& id) {
    std::mt19937 gen(std::random_device{}());
    ::caf::SRRecoParticle reco = CAFFiller<::caf::SRRecoParticle>::from_true(true_part, id);
    auto gaus_smear            = [&](float stddev) {
      return static_cast<float>(std::normal_distribution<float>(0.0f, stddev)(gen));
    };
    const float stddev_E = fake_reco_constants::E_res * static_cast<float>(std::sqrt(reco.E));
    //const float stddev_p = reco.p.Mag() * fake_reco_constants::p_res;

    reco.E = std::max(0.0f, reco.E + gaus_smear(stddev_E));
    //if (reco.p.Mag() > 0.0f) {
    //  const float smeared_mag = std::max(0.0f, reco.p.Mag() + gaus_smear(stddev_p));
    //  const float scale       = smeared_mag / reco.p.Mag();
    //  reco.p = ::caf::SRVector3D{reco.p.X() * scale, reco.p.Y() * scale, reco.p.Z() * scale}; // keep direction
    //}

    return reco;
  }

  ::caf::SRTrack CAFFiller<::caf::SRTrack>::from_true(const ::caf::SRTrueParticle& true_part,
                                                      const ::caf::TrueParticleID& id) {
    ::caf::SRTrack track{};

    track.start  = true_part.start_pos;
    track.end    = true_part.end_pos;
    track.dir    = direction_from_momentum(true_part);
    track.enddir = track.dir;
    track.time   = true_part.time;
    track.E      = gev_to_mev(true_part.p.E);
    track.Evis   = track.E;
    track.len_cm = distance(track.start, track.end);
    track.charge = charge_from_pdg(true_part.pdg);
    track.qual   = 1.0f;

    add_truth_match(track, id);
    return track;
  }

  ::caf::SRShower CAFFiller<::caf::SRShower>::from_true(const ::caf::SRTrueParticle& true_part,
                                                        const ::caf::TrueParticleID& id) {
    ::caf::SRShower shower{};

    shower.start     = true_part.start_pos;
    shower.direction = direction_from_momentum(true_part);
    shower.Evis      = gev_to_mev(true_part.p.E);

    add_truth_match(shower, id);
    return shower;
  }

  ::caf::SRInteraction CAFFiller<::caf::SRInteraction>::from_true(const ::caf::SRTrueInteraction& true_ixn,
                                                                  std::size_t truth_index) {
    ::caf::SRInteraction reco{};

    reco.id       = true_ixn.id;
    reco.vtx      = true_ixn.vtx;
    reco.Enu.calo = true_ixn.E;

    reco.truth.push_back(truth_index);
    reco.truthOverlap.push_back(1.0f);

    return reco;
  }

} // namespace sand
