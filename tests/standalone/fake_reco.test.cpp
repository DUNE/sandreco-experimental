#define BOOST_TEST_MODULE fake_reco
#include <boost/test/included/unit_test.hpp>

// Must include ufw first for UFW_ERROR macro
#include <ufw/utils.hpp>

// Include duneanaobj headers
#include <duneanaobj/StandardRecord/SRTrueInteraction.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>
#include <duneanaobj/StandardRecord/SRRecoParticle.h>
#include <duneanaobj/StandardRecord/SRTrack.h>
#include <duneanaobj/StandardRecord/SRShower.h>
#include <duneanaobj/StandardRecord/SRLorentzVector.h>

// Include fake_reco headers
#include <processes/fake_reco/genie_helpers/EvtCode_parser.hpp>
#include <processes/fake_reco/caf_handlers/tracks_showers.hpp>
#include <processes/fake_reco/caf_handlers/interactions.hpp>

#include <test_helpers.hpp>

#include <cmath>
#include <limits>

// ============================================================================
// Tests for EvtCode_parser.hpp - EventSummary
// ============================================================================

BOOST_AUTO_TEST_SUITE(EvtCode_parser_tests)

BOOST_AUTO_TEST_CASE(parse_cc_qes_event) {
  // Example: CC QES nu_mu on Ar40
  const std::string_view evt_code = "nu:14;tgt:1000180400;N:2112;proc:Weak[CC],QES";

  sand::fake_reco::EventSummary summary{evt_code};

  BOOST_TEST(summary.probe_pdg == 14);            // nu_mu
  BOOST_TEST(summary.target_pdg == 1000180400);   // Ar40
  BOOST_TEST(summary.hit_nucleon_pdg.has_value());
  BOOST_TEST(summary.hit_nucleon_pdg.value() == 2112);  // neutron
  BOOST_TEST(summary.interaction_type == "Weak[CC]");
  BOOST_TEST(summary.scattering_type == ::caf::kQE);
}

BOOST_AUTO_TEST_CASE(parse_cc_res_event) {
  // CC RES event with resonance
  const std::string_view evt_code = "nu:14;tgt:1000180400;N:2212;proc:Weak[CC],RES;res:0";

  sand::fake_reco::EventSummary summary{evt_code};

  BOOST_TEST(summary.probe_pdg == 14);
  BOOST_TEST(summary.target_pdg == 1000180400);
  BOOST_TEST(summary.hit_nucleon_pdg.value() == 2212);  // proton
  BOOST_TEST(summary.interaction_type == "Weak[CC]");
  BOOST_TEST(summary.scattering_type == ::caf::kRes);
  BOOST_TEST(summary.resonance_type.has_value());
  BOOST_TEST(summary.resonance_type.value() == sand::fake_reco::genie::kP33_1232);
}

BOOST_AUTO_TEST_CASE(parse_nc_dis_event) {
  // NC DIS event with quark info
  const std::string_view evt_code = "nu:-14;tgt:1000180400;N:2112;q:1(s);proc:Weak[NC],DIS";

  sand::fake_reco::EventSummary summary{evt_code};

  BOOST_TEST(summary.probe_pdg == -14);  // anti-nu_mu
  BOOST_TEST(summary.interaction_type == "Weak[NC]");
  BOOST_TEST(summary.scattering_type == ::caf::kDIS);
  BOOST_TEST(summary.hit_quark_pdg.has_value());
  BOOST_TEST(summary.hit_quark_pdg.value() == 1);  // down quark
  BOOST_TEST(summary.hit_sea_quark == true);
}

BOOST_AUTO_TEST_CASE(parse_valence_quark) {
  const std::string_view evt_code = "nu:14;tgt:1000180400;N:2212;q:2(v);proc:Weak[CC],DIS";

  sand::fake_reco::EventSummary summary{evt_code};

  BOOST_TEST(summary.hit_quark_pdg.value() == 2);  // up quark
  BOOST_TEST(summary.hit_sea_quark == false);      // valence
}

BOOST_AUTO_TEST_CASE(parse_charm_event) {
  const std::string_view evt_code = "nu:14;tgt:1000180400;proc:Weak[CC],DIS;charm:4122";

  sand::fake_reco::EventSummary summary{evt_code};

  BOOST_TEST(summary.is_charm_event == true);
  BOOST_TEST(summary.charmed_hadron_pdg.has_value());
  BOOST_TEST(summary.charmed_hadron_pdg.value() == 4122);  // Lambda_c
}

BOOST_AUTO_TEST_CASE(parse_charm_inclusive) {
  const std::string_view evt_code = "nu:14;tgt:1000180400;proc:Weak[CC],DIS;charm:incl";

  sand::fake_reco::EventSummary summary{evt_code};

  BOOST_TEST(summary.is_charm_event == true);
  BOOST_TEST(summary.charmed_hadron_pdg.has_value() == false);
}

BOOST_AUTO_TEST_CASE(parse_mec_event) {
  const std::string_view evt_code = "nu:14;tgt:1000180400;proc:Weak[CC],MEC";

  sand::fake_reco::EventSummary summary{evt_code};

  BOOST_TEST(summary.scattering_type == ::caf::kMEC);
}

BOOST_AUTO_TEST_CASE(parse_coh_event) {
  const std::string_view evt_code = "nu:14;tgt:1000180400;proc:Weak[CC],COH";

  sand::fake_reco::EventSummary summary{evt_code};

  BOOST_TEST(summary.scattering_type == ::caf::kCoh);
}

BOOST_AUTO_TEST_CASE(parse_hadron_multiplicity) {
  const std::string_view evt_code = "nu:14;tgt:1000180400;proc:Weak[CC],DIS;hmult:(p=2,n=1,pi+=1,pi-=0,pi0=1)";

  sand::fake_reco::EventSummary summary{evt_code};

  BOOST_TEST(summary.hmult.n_protons == 2);
  BOOST_TEST(summary.hmult.n_neutrons == 1);
  BOOST_TEST(summary.hmult.n_pi_plus == 1);
  BOOST_TEST(summary.hmult.n_pi_minus == 0);
  BOOST_TEST(summary.hmult.n_pi_zero == 1);
}

BOOST_AUTO_TEST_CASE(parse_electron_neutrino) {
  const std::string_view evt_code = "nu:12;tgt:1000180400;N:2112;proc:Weak[CC],QES";

  sand::fake_reco::EventSummary summary{evt_code};

  BOOST_TEST(summary.probe_pdg == 12);  // nu_e
}

BOOST_AUTO_TEST_CASE(parse_all_scattering_modes) {
  // Test that all known scattering modes can be parsed
  std::vector<std::pair<std::string, ::caf::ScatteringMode>> test_cases = {
      {"QES", ::caf::kQE},
      {"RES", ::caf::kRes},
      {"DIS", ::caf::kDIS},
      {"COH", ::caf::kCoh},
      {"MEC", ::caf::kMEC},
      {"DFR", ::caf::kDiffractive},
      {"NuEEL", ::caf::kNuElectronElastic},
      {"IMD", ::caf::kInvMuonDecay},
      {"CEvNS", ::caf::kCohElastic},
      {"IBD", ::caf::kInverseBetaDecay},
      {"GLR", ::caf::kGlashowResonance},
  };

  for (const auto& [mode_str, expected_mode] : test_cases) {
    std::string evt_code = "nu:14;tgt:1000180400;proc:Weak[CC]," + mode_str;
    sand::fake_reco::EventSummary summary{evt_code};
    BOOST_TEST(summary.scattering_type == expected_mode,
               "Failed for mode: " << mode_str);
  }
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Tests for tracks_showers.hpp - Classification functions
// ============================================================================

BOOST_AUTO_TEST_SUITE(tracks_showers_tests)

BOOST_AUTO_TEST_CASE(is_track_like_muon) {
  BOOST_TEST(sand::fake_reco::is_track_like(13) == true);   // mu-
  BOOST_TEST(sand::fake_reco::is_track_like(-13) == true);  // mu+
}

BOOST_AUTO_TEST_CASE(is_track_like_pion) {
  BOOST_TEST(sand::fake_reco::is_track_like(211) == true);   // pi+
  BOOST_TEST(sand::fake_reco::is_track_like(-211) == true);  // pi-
}

BOOST_AUTO_TEST_CASE(is_track_like_kaon) {
  BOOST_TEST(sand::fake_reco::is_track_like(321) == true);   // K+
  BOOST_TEST(sand::fake_reco::is_track_like(-321) == true);  // K-
}

BOOST_AUTO_TEST_CASE(is_track_like_proton) {
  BOOST_TEST(sand::fake_reco::is_track_like(2212) == true);  // proton
}

BOOST_AUTO_TEST_CASE(is_track_like_other) {
  BOOST_TEST(sand::fake_reco::is_track_like(11) == false);    // electron
  BOOST_TEST(sand::fake_reco::is_track_like(22) == false);    // photon
  BOOST_TEST(sand::fake_reco::is_track_like(111) == false);   // pi0
  BOOST_TEST(sand::fake_reco::is_track_like(2112) == false);  // neutron
}

BOOST_AUTO_TEST_CASE(is_shower_like_electron) {
  BOOST_TEST(sand::fake_reco::is_shower_like(11) == true);   // e-
  BOOST_TEST(sand::fake_reco::is_shower_like(-11) == true);  // e+
}

BOOST_AUTO_TEST_CASE(is_shower_like_photon) {
  BOOST_TEST(sand::fake_reco::is_shower_like(22) == true);
}

BOOST_AUTO_TEST_CASE(is_shower_like_pi0) {
  BOOST_TEST(sand::fake_reco::is_shower_like(111) == true);
}

BOOST_AUTO_TEST_CASE(is_shower_like_other) {
  BOOST_TEST(sand::fake_reco::is_shower_like(13) == false);    // muon
  BOOST_TEST(sand::fake_reco::is_shower_like(211) == false);   // pi+
  BOOST_TEST(sand::fake_reco::is_shower_like(2212) == false);  // proton
}

BOOST_AUTO_TEST_CASE(charge_from_pdg_positive) {
  BOOST_TEST(sand::fake_reco::charge_from_pdg(-11) == +1);   // positron
  BOOST_TEST(sand::fake_reco::charge_from_pdg(-13) == +1);   // anti-muon
  BOOST_TEST(sand::fake_reco::charge_from_pdg(211) == +1);   // pi+
  BOOST_TEST(sand::fake_reco::charge_from_pdg(321) == +1);   // K+
  BOOST_TEST(sand::fake_reco::charge_from_pdg(2212) == +1);  // proton
}

BOOST_AUTO_TEST_CASE(charge_from_pdg_negative) {
  BOOST_TEST(sand::fake_reco::charge_from_pdg(11) == -1);    // electron
  BOOST_TEST(sand::fake_reco::charge_from_pdg(13) == -1);    // muon
  BOOST_TEST(sand::fake_reco::charge_from_pdg(-211) == -1);  // pi-
  BOOST_TEST(sand::fake_reco::charge_from_pdg(-321) == -1);  // K-
  BOOST_TEST(sand::fake_reco::charge_from_pdg(-2212) == -1); // anti-proton
}

BOOST_AUTO_TEST_CASE(charge_from_pdg_neutral) {
  BOOST_TEST(sand::fake_reco::charge_from_pdg(22) == 0);    // photon
  BOOST_TEST(sand::fake_reco::charge_from_pdg(111) == 0);   // pi0
  BOOST_TEST(sand::fake_reco::charge_from_pdg(2112) == 0);  // neutron
  BOOST_TEST(sand::fake_reco::charge_from_pdg(14) == 0);    // nu_mu
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Tests for interactions.hpp - PDG counters
// ============================================================================

BOOST_AUTO_TEST_SUITE(interactions_tests)

BOOST_AUTO_TEST_CASE(update_pdg_counters_proton) {
  ::caf::SRTrueInteraction interaction{};
  sand::fake_reco::update_true_interaction_pdg_counters(interaction, 2212);
  BOOST_TEST(interaction.nproton == 1);
  BOOST_TEST(interaction.nneutron == 0);
  BOOST_TEST(interaction.npip == 0);
  BOOST_TEST(interaction.npim == 0);
  BOOST_TEST(interaction.npi0 == 0);
}

BOOST_AUTO_TEST_CASE(update_pdg_counters_neutron) {
  ::caf::SRTrueInteraction interaction{};
  sand::fake_reco::update_true_interaction_pdg_counters(interaction, 2112);
  BOOST_TEST(interaction.nneutron == 1);
}

BOOST_AUTO_TEST_CASE(update_pdg_counters_pions) {
  ::caf::SRTrueInteraction interaction{};
  sand::fake_reco::update_true_interaction_pdg_counters(interaction, 211);   // pi+
  sand::fake_reco::update_true_interaction_pdg_counters(interaction, -211);  // pi-
  sand::fake_reco::update_true_interaction_pdg_counters(interaction, 111);   // pi0

  BOOST_TEST(interaction.npip == 1);
  BOOST_TEST(interaction.npim == 1);
  BOOST_TEST(interaction.npi0 == 1);
}

BOOST_AUTO_TEST_CASE(update_pdg_counters_multiple) {
  ::caf::SRTrueInteraction interaction{};

  // Simulate a typical event: 2 protons, 1 neutron, 1 pi+
  sand::fake_reco::update_true_interaction_pdg_counters(interaction, 2212);
  sand::fake_reco::update_true_interaction_pdg_counters(interaction, 2212);
  sand::fake_reco::update_true_interaction_pdg_counters(interaction, 2112);
  sand::fake_reco::update_true_interaction_pdg_counters(interaction, 211);

  BOOST_TEST(interaction.nproton == 2);
  BOOST_TEST(interaction.nneutron == 1);
  BOOST_TEST(interaction.npip == 1);
}

BOOST_AUTO_TEST_CASE(update_pdg_counters_unknown) {
  ::caf::SRTrueInteraction interaction{};

  // Particles that shouldn't increment counters
  sand::fake_reco::update_true_interaction_pdg_counters(interaction, 13);   // muon
  sand::fake_reco::update_true_interaction_pdg_counters(interaction, 22);   // photon
  sand::fake_reco::update_true_interaction_pdg_counters(interaction, 321);  // kaon

  BOOST_TEST(interaction.nproton == 0);
  BOOST_TEST(interaction.nneutron == 0);
  BOOST_TEST(interaction.npip == 0);
  BOOST_TEST(interaction.npim == 0);
  BOOST_TEST(interaction.npi0 == 0);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Tests for SRTrack/SRShower/SRRecoParticle creation
// ============================================================================

BOOST_AUTO_TEST_SUITE(reco_object_creation_tests)

BOOST_AUTO_TEST_CASE(SRTrack_from_true_particle_basic) {
  ::caf::SRTrueParticle true_particle{};
  true_particle.pdg = 13;  // muon
  true_particle.p = TLorentzVector{0.0, 0.0, 1.0, 1.1};  // ~1 GeV/c along z
  true_particle.start_pos = ::caf::SRVector3D{0.0f, 0.0f, 0.0f};
  true_particle.end_pos = ::caf::SRVector3D{0.0f, 0.0f, 100.0f};  // 100 cm track
  true_particle.time = 0.5f;

  ::caf::TrueParticleID particle_id{};
  particle_id.ixn = 0;
  particle_id.type = ::caf::TrueParticleID::kPrimary;
  particle_id.part = 0;

  auto track = sand::fake_reco::SRTrack_from_true_particle(true_particle, particle_id);

  BOOST_TEST(track.start.x == 0.0f);
  BOOST_TEST(track.start.z == 0.0f);
  BOOST_TEST(track.end.z == 100.0f);
  BOOST_CHECK_CLOSE(track.len_cm, 100.0f, 0.01);
  BOOST_TEST(track.charge == -1);  // muon is negative
  BOOST_TEST(track.qual == 1.0f);
  BOOST_CHECK_CLOSE(track.E, 1100.0f, 0.01);  // 1.1 GeV -> 1100 MeV
  BOOST_TEST(track.truth.size() == 1);
  BOOST_TEST(track.truthOverlap[0] == 1.0f);
}

BOOST_AUTO_TEST_CASE(SRTrack_direction_normalized) {
  ::caf::SRTrueParticle true_particle{};
  true_particle.pdg = 2212;  // proton
  true_particle.p = TLorentzVector{3.0, 4.0, 0.0, 5.1};  // p = 5 GeV/c
  true_particle.start_pos = ::caf::SRVector3D{};
  true_particle.end_pos = ::caf::SRVector3D{};

  ::caf::TrueParticleID particle_id{};

  auto track = sand::fake_reco::SRTrack_from_true_particle(true_particle, particle_id);

  // Check direction is normalized
  float dir_mag = std::sqrt(track.dir.x * track.dir.x +
                            track.dir.y * track.dir.y +
                            track.dir.z * track.dir.z);
  BOOST_CHECK_CLOSE(dir_mag, 1.0f, 0.01);

  // Check direction is correct
  BOOST_CHECK_CLOSE(track.dir.x, 0.6f, 0.01);  // 3/5
  BOOST_CHECK_CLOSE(track.dir.y, 0.8f, 0.01);  // 4/5
}

BOOST_AUTO_TEST_CASE(SRShower_from_true_particle_basic) {
  ::caf::SRTrueParticle true_particle{};
  true_particle.pdg = 11;  // electron
  true_particle.p = TLorentzVector{0.0, 0.0, 0.5, 0.5};  // 0.5 GeV
  true_particle.start_pos = ::caf::SRVector3D{10.0f, 20.0f, 30.0f};

  ::caf::TrueParticleID particle_id{};
  particle_id.ixn = 1;
  particle_id.type = ::caf::TrueParticleID::kPrimary;
  particle_id.part = 2;

  auto shower = sand::fake_reco::SRShower_from_true_particle(true_particle, particle_id);

  BOOST_TEST(shower.start.x == 10.0f);
  BOOST_TEST(shower.start.y == 20.0f);
  BOOST_TEST(shower.start.z == 30.0f);
  BOOST_CHECK_CLOSE(shower.Evis, 500.0f, 0.01);  // 0.5 GeV -> 500 MeV
  BOOST_TEST(shower.truth.size() == 1);
  BOOST_TEST(shower.truthOverlap[0] == 1.0f);
}

BOOST_AUTO_TEST_CASE(SRRecoParticle_track_type) {
  ::caf::SRTrueParticle true_particle{};
  true_particle.pdg = 13;  // muon -> track
  true_particle.p = TLorentzVector{0.1, 0.2, 0.3, 0.5};
  true_particle.start_pos = ::caf::SRVector3D{1.0f, 2.0f, 3.0f};
  true_particle.end_pos = ::caf::SRVector3D{4.0f, 5.0f, 6.0f};

  ::caf::TrueParticleID particle_id{};

  auto reco = sand::fake_reco::SRRecoParticle_from_true_particle(true_particle, particle_id);

  BOOST_TEST(reco.primary == true);
  BOOST_TEST(reco.pdg == 13);
  BOOST_TEST(reco.score == 1.0f);
  BOOST_TEST(reco.E_method == ::caf::PartEMethod::kCalorimetry);
  BOOST_TEST(reco.origRecoObjType == ::caf::RecoObjType::kTrack);
  BOOST_CHECK_CLOSE(reco.E, 0.5f, 0.01);
  BOOST_TEST(reco.p.x == 0.1f);
  BOOST_TEST(reco.p.y == 0.2f);
  BOOST_TEST(reco.p.z == 0.3f);
}

BOOST_AUTO_TEST_CASE(SRRecoParticle_shower_type) {
  ::caf::SRTrueParticle true_particle{};
  true_particle.pdg = 11;  // electron -> shower
  true_particle.p = TLorentzVector{0.0, 0.0, 1.0, 1.0};

  ::caf::TrueParticleID particle_id{};

  auto reco = sand::fake_reco::SRRecoParticle_from_true_particle(true_particle, particle_id);

  BOOST_TEST(reco.origRecoObjType == ::caf::RecoObjType::kShower);
}

BOOST_AUTO_TEST_SUITE_END()

// Final test to handle cleanup
BOOST_AUTO_TEST_CASE(dummy_exit) { sand::test_exit(); }
