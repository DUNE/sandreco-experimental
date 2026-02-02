#define BOOST_TEST_MODULE evtcode_parser
#include <boost/test/included/unit_test.hpp>

#include <processes/fake_reco/genie_helpers/EvtCode_parser.hpp>
#include <test_helpers.hpp>

using namespace sand;

// =============================================================================
// Basic Parsing Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(basic_parsing)

BOOST_AUTO_TEST_CASE(parse_neutrino_pdg) {
  EventSummary evt{"nu:14;tgt:1000180400;proc:Weak[CC],QES"};
  BOOST_TEST(evt.probe_pdg == 14);
}

BOOST_AUTO_TEST_CASE(parse_antineutrino_pdg) {
  EventSummary evt{"nu:-14;tgt:1000180400;proc:Weak[CC],QES"};
  BOOST_TEST(evt.probe_pdg == -14);
}

BOOST_AUTO_TEST_CASE(parse_electron_neutrino) {
  EventSummary evt{"nu:12;tgt:1000060120;proc:Weak[CC],QES"};
  BOOST_TEST(evt.probe_pdg == 12);
}

BOOST_AUTO_TEST_CASE(parse_target_pdg) {
  EventSummary evt{"nu:14;tgt:1000180400;proc:Weak[CC],QES"};
  BOOST_TEST(evt.target_pdg == 1000180400);  // Argon-40
}

BOOST_AUTO_TEST_CASE(parse_carbon_target) {
  EventSummary evt{"nu:14;tgt:1000060120;proc:Weak[CC],QES"};
  BOOST_TEST(evt.target_pdg == 1000060120);  // Carbon-12
}

BOOST_AUTO_TEST_CASE(parse_hit_nucleon_proton) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2212;proc:Weak[CC],QES"};
  BOOST_TEST(evt.hit_nucleon_pdg.has_value());
  BOOST_TEST(evt.hit_nucleon_pdg.value() == 2212);
}

BOOST_AUTO_TEST_CASE(parse_hit_nucleon_neutron) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],QES"};
  BOOST_TEST(evt.hit_nucleon_pdg.has_value());
  BOOST_TEST(evt.hit_nucleon_pdg.value() == 2112);
}

BOOST_AUTO_TEST_CASE(no_hit_nucleon_for_coherent) {
  EventSummary evt{"nu:14;tgt:1000180400;proc:Weak[CC],COH"};
  BOOST_TEST(!evt.hit_nucleon_pdg.has_value());
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// Interaction Type Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(interaction_types)

BOOST_AUTO_TEST_CASE(parse_cc_interaction) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],QES"};
  BOOST_TEST(evt.interaction_type == "Weak[CC]");
}

BOOST_AUTO_TEST_CASE(parse_nc_interaction) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2212;proc:Weak[NC],QES"};
  BOOST_TEST(evt.interaction_type == "Weak[NC]");
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// Scattering Mode Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(scattering_modes)

BOOST_AUTO_TEST_CASE(parse_qes_mode) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],QES"};
  BOOST_TEST(evt.scattering_type == ::caf::kQE);
}

BOOST_AUTO_TEST_CASE(parse_res_mode) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2212;proc:Weak[CC],RES"};
  BOOST_TEST(evt.scattering_type == ::caf::kRes);
}

BOOST_AUTO_TEST_CASE(parse_dis_mode) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],DIS"};
  BOOST_TEST(evt.scattering_type == ::caf::kDIS);
}

BOOST_AUTO_TEST_CASE(parse_mec_mode) {
  EventSummary evt{"nu:14;tgt:1000180400;proc:Weak[CC],MEC"};
  BOOST_TEST(evt.scattering_type == ::caf::kMEC);
}

BOOST_AUTO_TEST_CASE(parse_coh_mode) {
  EventSummary evt{"nu:14;tgt:1000180400;proc:Weak[CC],COH"};
  BOOST_TEST(evt.scattering_type == ::caf::kCoh);
}

BOOST_AUTO_TEST_CASE(parse_dfr_mode) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2212;proc:Weak[CC],DFR"};
  BOOST_TEST(evt.scattering_type == ::caf::kDiffractive);
}

BOOST_AUTO_TEST_CASE(parse_single_kaon_mode) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],1Kaon"};
  BOOST_TEST(evt.scattering_type == ::caf::kSingleKaon);
}

BOOST_AUTO_TEST_CASE(parse_nueel_mode) {
  EventSummary evt{"nu:14;tgt:11;proc:Weak[CC],NuEEL"};
  BOOST_TEST(evt.scattering_type == ::caf::kNuElectronElastic);
}

BOOST_AUTO_TEST_CASE(parse_imd_mode) {
  EventSummary evt{"nu:14;tgt:13;proc:Weak[CC],IMD"};
  BOOST_TEST(evt.scattering_type == ::caf::kInvMuonDecay);
}

BOOST_AUTO_TEST_CASE(parse_imdanh_mode) {
  EventSummary evt{"nu:-14;tgt:11;proc:Weak[CC],IMDAnh"};
  BOOST_TEST(evt.scattering_type == ::caf::kIMDAnnihilation);
}

BOOST_AUTO_TEST_CASE(parse_amnugamma_mode) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[NC],AMNuGamma"};
  BOOST_TEST(evt.scattering_type == ::caf::kAMNuGamma);
}

BOOST_AUTO_TEST_CASE(parse_cevns_mode) {
  EventSummary evt{"nu:14;tgt:1000180400;proc:Weak[NC],CEvNS"};
  BOOST_TEST(evt.scattering_type == ::caf::kCohElastic);
}

BOOST_AUTO_TEST_CASE(parse_ibd_mode) {
  EventSummary evt{"nu:-12;tgt:2212;proc:Weak[CC],IBD"};
  BOOST_TEST(evt.scattering_type == ::caf::kInverseBetaDecay);
}

BOOST_AUTO_TEST_CASE(parse_glr_mode) {
  EventSummary evt{"nu:-12;tgt:11;proc:Weak[CC],GLR"};
  BOOST_TEST(evt.scattering_type == ::caf::kGlashowResonance);
}

BOOST_AUTO_TEST_CASE(parse_photon_coh_mode) {
  EventSummary evt{"nu:22;tgt:1000180400;proc:EM,PhotonCOH"};
  BOOST_TEST(evt.scattering_type == ::caf::kPhotonCoh);
}

BOOST_AUTO_TEST_CASE(parse_photon_res_mode) {
  EventSummary evt{"nu:22;tgt:1000180400;N:2212;proc:EM,PhotonRES"};
  BOOST_TEST(evt.scattering_type == ::caf::kPhotonRes);
}

BOOST_AUTO_TEST_CASE(parse_dm_elastic_mode) {
  EventSummary evt{"nu:1000022;tgt:1000180400;N:2212;proc:DarkMatter,DMEL"};
  BOOST_TEST(evt.scattering_type == ::caf::kDarkMatterElastic);
}

BOOST_AUTO_TEST_CASE(parse_dm_dis_mode) {
  EventSummary evt{"nu:1000022;tgt:1000180400;N:2112;proc:DarkMatter,DMDIS"};
  BOOST_TEST(evt.scattering_type == ::caf::kDarkMatterDIS);
}

BOOST_AUTO_TEST_CASE(parse_dm_electron_mode) {
  EventSummary evt{"nu:1000022;tgt:11;proc:DarkMatter,DME"};
  BOOST_TEST(evt.scattering_type == ::caf::kDarkMatterElectron);
}

BOOST_AUTO_TEST_CASE(parse_unknown_mode) {
  EventSummary evt{"nu:14;tgt:1000180400;proc:Weak[CC],Unknown"};
  BOOST_TEST(evt.scattering_type == ::caf::kUnknownMode);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// Quark Tests (DIS)
// =============================================================================

BOOST_AUTO_TEST_SUITE(quark_parsing)

BOOST_AUTO_TEST_CASE(parse_sea_quark) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;q:1(s);proc:Weak[CC],DIS"};
  BOOST_TEST(evt.hit_quark_pdg.has_value());
  BOOST_TEST(evt.hit_quark_pdg.value() == 1);
  BOOST_TEST(evt.hit_sea_quark == true);
}

BOOST_AUTO_TEST_CASE(parse_valence_quark) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;q:2(v);proc:Weak[CC],DIS"};
  BOOST_TEST(evt.hit_quark_pdg.has_value());
  BOOST_TEST(evt.hit_quark_pdg.value() == 2);
  BOOST_TEST(evt.hit_sea_quark == false);
}

BOOST_AUTO_TEST_CASE(parse_down_quark_sea) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2212;q:1(s);proc:Weak[CC],DIS"};
  BOOST_TEST(evt.hit_quark_pdg.value() == 1);  // d quark
  BOOST_TEST(evt.hit_sea_quark == true);
}

BOOST_AUTO_TEST_CASE(parse_strange_quark) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2212;q:3(s);proc:Weak[CC],DIS"};
  BOOST_TEST(evt.hit_quark_pdg.value() == 3);  // s quark
}

BOOST_AUTO_TEST_CASE(no_quark_for_qes) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],QES"};
  BOOST_TEST(!evt.hit_quark_pdg.has_value());
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// Charm Production Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(charm_production)

BOOST_AUTO_TEST_CASE(charm_inclusive) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],DIS;charm:incl"};
  BOOST_TEST(evt.is_charm_event == true);
  BOOST_TEST(!evt.charmed_hadron_pdg.has_value());
}

BOOST_AUTO_TEST_CASE(charm_d_plus) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],DIS;charm:411"};
  BOOST_TEST(evt.is_charm_event == true);
  BOOST_TEST(evt.charmed_hadron_pdg.has_value());
  BOOST_TEST(evt.charmed_hadron_pdg.value() == 411);  // D+
}

BOOST_AUTO_TEST_CASE(charm_d_zero) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],DIS;charm:421"};
  BOOST_TEST(evt.charmed_hadron_pdg.value() == 421);  // D0
}

BOOST_AUTO_TEST_CASE(charm_lambda_c) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],DIS;charm:4122"};
  BOOST_TEST(evt.charmed_hadron_pdg.value() == 4122);  // Lambda_c+
}

BOOST_AUTO_TEST_CASE(no_charm) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],QES"};
  BOOST_TEST(evt.is_charm_event == false);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// Strange Production Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(strange_production)

BOOST_AUTO_TEST_CASE(strange_inclusive) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],DIS;strange:incl"};
  BOOST_TEST(evt.is_strange_event == true);
  BOOST_TEST(!evt.strange_hadron_pdg.has_value());
}

BOOST_AUTO_TEST_CASE(strange_k_plus) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],1Kaon;strange:321"};
  BOOST_TEST(evt.is_strange_event == true);
  BOOST_TEST(evt.strange_hadron_pdg.value() == 321);  // K+
}

BOOST_AUTO_TEST_CASE(strange_lambda) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],DIS;strange:3122"};
  BOOST_TEST(evt.strange_hadron_pdg.value() == 3122);  // Lambda
}

BOOST_AUTO_TEST_CASE(no_strange) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],QES"};
  BOOST_TEST(evt.is_strange_event == false);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// Resonance Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(resonances)

BOOST_AUTO_TEST_CASE(parse_delta_1232) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2212;proc:Weak[CC],RES;res:0"};
  BOOST_TEST(evt.resonance_type.has_value());
  BOOST_TEST(evt.resonance_type.value() == genie::kP33_1232);
}

BOOST_AUTO_TEST_CASE(parse_n1535_s11) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],RES;res:1"};
  BOOST_TEST(evt.resonance_type.value() == genie::kS11_1535);
}

BOOST_AUTO_TEST_CASE(parse_n1520_d13) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2212;proc:Weak[CC],RES;res:2"};
  BOOST_TEST(evt.resonance_type.value() == genie::kD13_1520);
}

BOOST_AUTO_TEST_CASE(parse_roper_resonance) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],RES;res:8"};
  BOOST_TEST(evt.resonance_type.value() == genie::kP11_1440);
}

BOOST_AUTO_TEST_CASE(parse_delta_1950) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2212;proc:Weak[CC],RES;res:15"};
  BOOST_TEST(evt.resonance_type.value() == genie::kF37_1950);
}

BOOST_AUTO_TEST_CASE(no_resonance_for_qes) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],QES"};
  BOOST_TEST(!evt.resonance_type.has_value());
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// Decay Mode Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(decay_modes)

BOOST_AUTO_TEST_CASE(parse_decay_mode_0) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2212;proc:Weak[CC],RES;res:0;dec:0"};
  BOOST_TEST(evt.decay_mode.has_value());
  BOOST_TEST(evt.decay_mode.value() == 0);
}

BOOST_AUTO_TEST_CASE(parse_decay_mode_1) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2212;proc:Weak[CC],RES;res:0;dec:1"};
  BOOST_TEST(evt.decay_mode.value() == 1);
}

BOOST_AUTO_TEST_CASE(parse_decay_mode_2) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2212;proc:Weak[CC],RES;res:0;dec:2"};
  BOOST_TEST(evt.decay_mode.value() == 2);
}

BOOST_AUTO_TEST_CASE(no_decay_for_qes) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],QES"};
  BOOST_TEST(!evt.decay_mode.has_value());
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// Final State Quark/Lepton Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(final_state)

BOOST_AUTO_TEST_CASE(parse_final_quark) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;q:1(v);proc:Weak[CC],DIS;finalquark:2"};
  BOOST_TEST(evt.final_quark_pdg.has_value());
  BOOST_TEST(evt.final_quark_pdg.value() == 2);  // u quark
}

BOOST_AUTO_TEST_CASE(parse_final_lepton_muon) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],QES;finallepton:13"};
  BOOST_TEST(evt.final_lepton_pdg.has_value());
  BOOST_TEST(evt.final_lepton_pdg.value() == 13);  // muon
}

BOOST_AUTO_TEST_CASE(parse_final_lepton_electron) {
  EventSummary evt{"nu:12;tgt:1000180400;N:2112;proc:Weak[CC],QES;finallepton:11"};
  BOOST_TEST(evt.final_lepton_pdg.value() == 11);  // electron
}

BOOST_AUTO_TEST_CASE(parse_final_lepton_tau) {
  EventSummary evt{"nu:16;tgt:1000180400;N:2112;proc:Weak[CC],QES;finallepton:15"};
  BOOST_TEST(evt.final_lepton_pdg.value() == 15);  // tau
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// Hadron Multiplicity Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(hadron_multiplicity)

BOOST_AUTO_TEST_CASE(parse_hmult_protons) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],DIS;hmult:(p=2,n=1)"};
  BOOST_TEST(evt.hmult.n_protons == 2);
  BOOST_TEST(evt.hmult.n_neutrons == 1);
}

BOOST_AUTO_TEST_CASE(parse_hmult_pions) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],DIS;hmult:(pi+=1,pi-=2,pi0=0)"};
  BOOST_TEST(evt.hmult.n_pi_plus == 1);
  BOOST_TEST(evt.hmult.n_pi_minus == 2);
  BOOST_TEST(evt.hmult.n_pi_zero == 0);
}

BOOST_AUTO_TEST_CASE(parse_hmult_full) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],DIS;hmult:(p=1,n=2,pi+=1,pi-=0,pi0=1,gamma=2)"};
  BOOST_TEST(evt.hmult.n_protons == 1);
  BOOST_TEST(evt.hmult.n_neutrons == 2);
  BOOST_TEST(evt.hmult.n_pi_plus == 1);
  BOOST_TEST(evt.hmult.n_pi_minus == 0);
  BOOST_TEST(evt.hmult.n_pi_zero == 1);
  BOOST_TEST(evt.hmult.n_gammas == 2);
}

BOOST_AUTO_TEST_CASE(parse_hmult_rho_mesons) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],DIS;hmult:(rho+=1,rho-=0,rho0=1)"};
  BOOST_TEST(evt.hmult.n_rho_plus == 1);
  BOOST_TEST(evt.hmult.n_rho_minus == 0);
  BOOST_TEST(evt.hmult.n_rho_zero == 1);
}

BOOST_AUTO_TEST_CASE(default_hmult_zero) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],QES"};
  BOOST_TEST(evt.hmult.n_protons == 0);
  BOOST_TEST(evt.hmult.n_neutrons == 0);
  BOOST_TEST(evt.hmult.n_pi_plus == 0);
  BOOST_TEST(evt.hmult.n_pi_minus == 0);
  BOOST_TEST(evt.hmult.n_pi_zero == 0);
  BOOST_TEST(evt.hmult.n_gammas == 0);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// Complex Event Strings Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(complex_events)

BOOST_AUTO_TEST_CASE(full_ccqe_event) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;proc:Weak[CC],QES;finallepton:13"};
  BOOST_TEST(evt.probe_pdg == 14);
  BOOST_TEST(evt.target_pdg == 1000180400);
  BOOST_TEST(evt.hit_nucleon_pdg.value() == 2112);
  BOOST_TEST(evt.interaction_type == "Weak[CC]");
  BOOST_TEST(evt.scattering_type == ::caf::kQE);
  BOOST_TEST(evt.final_lepton_pdg.value() == 13);
}

BOOST_AUTO_TEST_CASE(full_ccres_event) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2212;proc:Weak[CC],RES;res:0;dec:1;hmult:(p=1,pi+=1)"};
  BOOST_TEST(evt.probe_pdg == 14);
  BOOST_TEST(evt.hit_nucleon_pdg.value() == 2212);
  BOOST_TEST(evt.scattering_type == ::caf::kRes);
  BOOST_TEST(evt.resonance_type.value() == genie::kP33_1232);
  BOOST_TEST(evt.decay_mode.value() == 1);
  BOOST_TEST(evt.hmult.n_protons == 1);
  BOOST_TEST(evt.hmult.n_pi_plus == 1);
}

BOOST_AUTO_TEST_CASE(full_ccdis_charm_event) {
  EventSummary evt{"nu:14;tgt:1000180400;N:2112;q:3(s);proc:Weak[CC],DIS;charm:411;finalquark:4"};
  BOOST_TEST(evt.probe_pdg == 14);
  BOOST_TEST(evt.hit_nucleon_pdg.value() == 2112);
  BOOST_TEST(evt.hit_quark_pdg.value() == 3);
  BOOST_TEST(evt.hit_sea_quark == true);
  BOOST_TEST(evt.scattering_type == ::caf::kDIS);
  BOOST_TEST(evt.is_charm_event == true);
  BOOST_TEST(evt.charmed_hadron_pdg.value() == 411);
  BOOST_TEST(evt.final_quark_pdg.value() == 4);
}

BOOST_AUTO_TEST_CASE(nccoh_event) {
  EventSummary evt{"nu:14;tgt:1000180400;proc:Weak[NC],COH"};
  BOOST_TEST(evt.probe_pdg == 14);
  BOOST_TEST(evt.interaction_type == "Weak[NC]");
  BOOST_TEST(evt.scattering_type == ::caf::kCoh);
  BOOST_TEST(!evt.hit_nucleon_pdg.has_value());
}

BOOST_AUTO_TEST_CASE(antineutrino_ccqe_event) {
  EventSummary evt{"nu:-14;tgt:1000180400;N:2212;proc:Weak[CC],QES;finallepton:-13"};
  BOOST_TEST(evt.probe_pdg == -14);
  BOOST_TEST(evt.hit_nucleon_pdg.value() == 2212);
  BOOST_TEST(evt.final_lepton_pdg.value() == -13);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// Edge Cases and Error Handling
// =============================================================================

BOOST_AUTO_TEST_SUITE(edge_cases)

BOOST_AUTO_TEST_CASE(empty_string) {
  EventSummary evt{""};
  BOOST_TEST(evt.probe_pdg == 0);
  BOOST_TEST(evt.target_pdg == 0);
  BOOST_TEST(evt.scattering_type == ::caf::ScatteringMode{});
}

BOOST_AUTO_TEST_CASE(extra_semicolons) {
  EventSummary evt{";;nu:14;;tgt:1000180400;;proc:Weak[CC],QES;;"};
  BOOST_TEST(evt.probe_pdg == 14);
  BOOST_TEST(evt.target_pdg == 1000180400);
  BOOST_TEST(evt.scattering_type == ::caf::kQE);
}

BOOST_AUTO_TEST_CASE(tokens_in_different_order) {
  EventSummary evt{"proc:Weak[CC],QES;N:2112;tgt:1000180400;nu:14"};
  BOOST_TEST(evt.probe_pdg == 14);
  BOOST_TEST(evt.target_pdg == 1000180400);
  BOOST_TEST(evt.hit_nucleon_pdg.value() == 2112);
  BOOST_TEST(evt.scattering_type == ::caf::kQE);
}

BOOST_AUTO_TEST_CASE(unknown_tokens_ignored) {
  EventSummary evt{"nu:14;unknown:value;tgt:1000180400;proc:Weak[CC],QES"};
  BOOST_TEST(evt.probe_pdg == 14);
  BOOST_TEST(evt.target_pdg == 1000180400);
}

BOOST_AUTO_TEST_SUITE_END()

FIX_TEST_EXIT
