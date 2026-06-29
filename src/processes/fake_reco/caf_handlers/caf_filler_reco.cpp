#include "caf_filler.hpp"
#include "utils/GlucksternSmearing.hpp"
#include "utils/GaussSmearing.hpp"

namespace sand {

  namespace {
    [[nodiscard]] ::caf::SRVector3D direction_from_momentum(const ::caf::SRTrueParticle& p) {
      return normalize_to_direction(p.p.px, p.p.py, p.p.pz);
    }
  } // namespace

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

  ::caf::SRRecoParticle CAFFiller<::caf::SRRecoParticle>::from_true_with_gauss_smearing(const ::caf::SRTrueParticle& true_part,
                                                                    const ::caf::TrueParticleID& id, const double en_res, const double p_res, 
                                                                    const double x_res, const double y_res, const double z_res) {

    auto reco = CAFFiller<::caf::SRRecoParticle>::from_true(true_part, id);

    auto gauss_helper = GaussSmearing();
    // needed since reco.p is a SRVector3D instead true_part.p is a SRLorenztVector
    const ::caf::SRVector3D true_part_mom_vec{true_part.p.px, true_part.p.py, true_part.p.pz};
    
    reco.E        = gauss_helper.apply_energy_smearing(true_part.p.E, en_res);
    reco.p        = gauss_helper.apply_momentum_smearing(true_part.p, p_res);
    reco.start    = gauss_helper.apply_pos_smearing(true_part.start_pos, x_res, y_res, z_res);
    reco.end      = gauss_helper.apply_pos_smearing(true_part.end_pos, x_res, y_res, z_res);

    if (is_track_like(true_part.pdg)) {
      reco.origRecoObjType = ::caf::RecoObjType::kTrack;
    } else if (is_shower_like(true_part.pdg)) {
      reco.origRecoObjType = ::caf::RecoObjType::kShower;
    }
    // else the particle is neutral cannot be reconstructed

    add_truth_match(reco, id);
    return reco;
  }

  ::caf::SRRecoParticle CAFFiller<::caf::SRRecoParticle>::from_true_with_mu_smearing(
      const ::caf::SRTrueParticle& true_part, const ::caf::TrueParticleID& id, const EDEPTrajectory& true_part_trj,
      double intrinsic_pos_res_t, double intrinsic_pos_res_l, double hit_energy_thr, double b_field_magnitude) {
    // start by filling all fields from truth
    auto reco = CAFFiller<::caf::SRRecoParticle>::from_true(true_part, id);
    // return from_true for all particle except muons (temporarily)
    if (std::abs(true_part.pdg) != 13) {
      return reco;
    }

    // for muons in the drift tracker (temporarily) smear according to the Gluckstern formula
    const auto& hit_map = true_part_trj.GetHitMap();
    const auto& it      = hit_map.find(component::DRIFT);
    if (it == hit_map.end()) {
      return reco;
    }
    const auto& hit_vec = it->second;

    const auto gluckstern_helper = GlucksternSmearing{hit_vec, hit_energy_thr};

    // apply the measurement resolution smearing
    if (!gluckstern_helper.IsValid()) {
      return reco;
    }

    reco.p = gluckstern_helper.apply_smearing(true_part.p, intrinsic_pos_res_t, intrinsic_pos_res_l, b_field_magnitude);

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
