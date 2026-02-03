#define BOOST_TEST_MODULE caf_filler_common
#include <boost/test/included/unit_test.hpp>

#include <processes/fake_reco/caf_handlers/caf_filler_common.hpp>
#include <test_helpers.hpp>

#include <duneanaobj/StandardRecord/SRRecoParticle.h>
#include <duneanaobj/StandardRecord/SRShower.h>
#include <duneanaobj/StandardRecord/SRTrack.h>

#include <cmath>

using namespace sand;

// =============================================================================
// PDG Info Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(pdg_info)

BOOST_AUTO_TEST_CASE(get_pdg_info_valid) {
  const auto* info = get_pdg_info(13);  // muon
  BOOST_TEST(info != nullptr);
}

BOOST_AUTO_TEST_CASE(get_pdg_info_invalid) {
  const auto* info = get_pdg_info(9999999);  // invalid PDG
  BOOST_TEST(info == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// Charge from PDG Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(charge_tests)

BOOST_AUTO_TEST_CASE(charge_positive_muon) {
  BOOST_TEST(charge_from_pdg(-13) == +1);  // mu+
}

BOOST_AUTO_TEST_CASE(charge_negative_muon) {
  BOOST_TEST(charge_from_pdg(13) == -1);  // mu-
}

BOOST_AUTO_TEST_CASE(charge_proton) {
  BOOST_TEST(charge_from_pdg(2212) == +1);
}

BOOST_AUTO_TEST_CASE(charge_neutron) {
  BOOST_TEST(charge_from_pdg(2112) == 0);
}

BOOST_AUTO_TEST_CASE(charge_pi_plus) {
  BOOST_TEST(charge_from_pdg(211) == +1);
}

BOOST_AUTO_TEST_CASE(charge_pi_minus) {
  BOOST_TEST(charge_from_pdg(-211) == -1);
}

BOOST_AUTO_TEST_CASE(charge_pi_zero) {
  BOOST_TEST(charge_from_pdg(111) == 0);
}

BOOST_AUTO_TEST_CASE(charge_electron) {
  BOOST_TEST(charge_from_pdg(11) == -1);
}

BOOST_AUTO_TEST_CASE(charge_positron) {
  BOOST_TEST(charge_from_pdg(-11) == +1);
}

BOOST_AUTO_TEST_CASE(charge_gamma) {
  BOOST_TEST(charge_from_pdg(22) == 0);
}

BOOST_AUTO_TEST_CASE(charge_kaon_plus) {
  BOOST_TEST(charge_from_pdg(321) == +1);
}

BOOST_AUTO_TEST_CASE(charge_kaon_minus) {
  BOOST_TEST(charge_from_pdg(-321) == -1);
}

BOOST_AUTO_TEST_CASE(charge_invalid_pdg) {
  BOOST_TEST(charge_from_pdg(9999999) == 0);  // invalid -> 0
}

BOOST_AUTO_TEST_CASE(charge_neutrino) {
  BOOST_TEST(charge_from_pdg(14) == 0);  // nu_mu
  BOOST_TEST(charge_from_pdg(12) == 0);  // nu_e
  BOOST_TEST(charge_from_pdg(16) == 0);  // nu_tau
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// Lepton Classification Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(lepton_tests)

BOOST_AUTO_TEST_CASE(is_lepton_muon) {
  BOOST_TEST(is_lepton_pdg(13) == true);
  BOOST_TEST(is_lepton_pdg(-13) == true);
}

BOOST_AUTO_TEST_CASE(is_lepton_electron) {
  BOOST_TEST(is_lepton_pdg(11) == true);
  BOOST_TEST(is_lepton_pdg(-11) == true);
}

BOOST_AUTO_TEST_CASE(is_lepton_tau) {
  BOOST_TEST(is_lepton_pdg(15) == true);
  BOOST_TEST(is_lepton_pdg(-15) == true);
}

BOOST_AUTO_TEST_CASE(is_lepton_neutrinos) {
  BOOST_TEST(is_lepton_pdg(12) == true);   // nu_e
  BOOST_TEST(is_lepton_pdg(-12) == true);  // anti-nu_e
  BOOST_TEST(is_lepton_pdg(14) == true);   // nu_mu
  BOOST_TEST(is_lepton_pdg(-14) == true);  // anti-nu_mu
  BOOST_TEST(is_lepton_pdg(16) == true);   // nu_tau
  BOOST_TEST(is_lepton_pdg(-16) == true);  // anti-nu_tau
}

BOOST_AUTO_TEST_CASE(is_lepton_not_proton) {
  BOOST_TEST(is_lepton_pdg(2212) == false);
}

BOOST_AUTO_TEST_CASE(is_lepton_not_pion) {
  BOOST_TEST(is_lepton_pdg(211) == false);
  BOOST_TEST(is_lepton_pdg(-211) == false);
  BOOST_TEST(is_lepton_pdg(111) == false);
}

BOOST_AUTO_TEST_CASE(is_lepton_not_gamma) {
  BOOST_TEST(is_lepton_pdg(22) == false);
}

BOOST_AUTO_TEST_CASE(is_charged_lepton_muon) {
  BOOST_TEST(is_charged_lepton_pdg(13) == true);
  BOOST_TEST(is_charged_lepton_pdg(-13) == true);
}

BOOST_AUTO_TEST_CASE(is_charged_lepton_electron) {
  BOOST_TEST(is_charged_lepton_pdg(11) == true);
  BOOST_TEST(is_charged_lepton_pdg(-11) == true);
}

BOOST_AUTO_TEST_CASE(is_charged_lepton_tau) {
  BOOST_TEST(is_charged_lepton_pdg(15) == true);
  BOOST_TEST(is_charged_lepton_pdg(-15) == true);
}

BOOST_AUTO_TEST_CASE(is_charged_lepton_not_neutrino) {
  BOOST_TEST(is_charged_lepton_pdg(12) == false);
  BOOST_TEST(is_charged_lepton_pdg(14) == false);
  BOOST_TEST(is_charged_lepton_pdg(16) == false);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// Track-like / Shower-like Classification Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(particle_topology)

BOOST_AUTO_TEST_CASE(track_like_muon) {
  BOOST_TEST(is_track_like(13) == true);
  BOOST_TEST(is_track_like(-13) == true);
}

BOOST_AUTO_TEST_CASE(track_like_pion_charged) {
  BOOST_TEST(is_track_like(211) == true);
  BOOST_TEST(is_track_like(-211) == true);
}

BOOST_AUTO_TEST_CASE(track_like_kaon_charged) {
  BOOST_TEST(is_track_like(321) == true);
  BOOST_TEST(is_track_like(-321) == true);
}

BOOST_AUTO_TEST_CASE(track_like_proton) {
  BOOST_TEST(is_track_like(2212) == true);
  BOOST_TEST(is_track_like(-2212) == true);  // antiproton
}

BOOST_AUTO_TEST_CASE(track_like_not_electron) {
  BOOST_TEST(is_track_like(11) == false);
  BOOST_TEST(is_track_like(-11) == false);
}

BOOST_AUTO_TEST_CASE(track_like_not_neutron) {
  BOOST_TEST(is_track_like(2112) == false);
}

BOOST_AUTO_TEST_CASE(track_like_not_gamma) {
  BOOST_TEST(is_track_like(22) == false);
}

BOOST_AUTO_TEST_CASE(track_like_not_pi0) {
  BOOST_TEST(is_track_like(111) == false);
}

BOOST_AUTO_TEST_CASE(shower_like_electron) {
  BOOST_TEST(is_shower_like(11) == true);
  BOOST_TEST(is_shower_like(-11) == true);
}

BOOST_AUTO_TEST_CASE(shower_like_gamma) {
  BOOST_TEST(is_shower_like(22) == true);
}

BOOST_AUTO_TEST_CASE(shower_like_pi0) {
  BOOST_TEST(is_shower_like(111) == true);
}

BOOST_AUTO_TEST_CASE(shower_like_not_muon) {
  BOOST_TEST(is_shower_like(13) == false);
}

BOOST_AUTO_TEST_CASE(shower_like_not_proton) {
  BOOST_TEST(is_shower_like(2212) == false);
}

BOOST_AUTO_TEST_CASE(shower_like_not_charged_pion) {
  BOOST_TEST(is_shower_like(211) == false);
  BOOST_TEST(is_shower_like(-211) == false);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// Vector Math Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(vector_math)

BOOST_AUTO_TEST_CASE(normalize_unit_x) {
  auto dir = normalize_to_direction(1.0f, 0.0f, 0.0f);
  BOOST_CHECK_CLOSE(dir.x, 1.0f, 1e-5);
  BOOST_CHECK_CLOSE(dir.y, 0.0f, 1e-5);
  BOOST_CHECK_CLOSE(dir.z, 0.0f, 1e-5);
}

BOOST_AUTO_TEST_CASE(normalize_unit_y) {
  auto dir = normalize_to_direction(0.0f, 2.0f, 0.0f);
  BOOST_CHECK_CLOSE(dir.x, 0.0f, 1e-5);
  BOOST_CHECK_CLOSE(dir.y, 1.0f, 1e-5);
  BOOST_CHECK_CLOSE(dir.z, 0.0f, 1e-5);
}

BOOST_AUTO_TEST_CASE(normalize_unit_z) {
  auto dir = normalize_to_direction(0.0f, 0.0f, 3.0f);
  BOOST_CHECK_CLOSE(dir.x, 0.0f, 1e-5);
  BOOST_CHECK_CLOSE(dir.y, 0.0f, 1e-5);
  BOOST_CHECK_CLOSE(dir.z, 1.0f, 1e-5);
}

BOOST_AUTO_TEST_CASE(normalize_diagonal) {
  auto dir = normalize_to_direction(1.0f, 1.0f, 1.0f);
  const float expected = 1.0f / std::sqrt(3.0f);
  BOOST_CHECK_CLOSE(dir.x, expected, 1e-4);
  BOOST_CHECK_CLOSE(dir.y, expected, 1e-4);
  BOOST_CHECK_CLOSE(dir.z, expected, 1e-4);
}

BOOST_AUTO_TEST_CASE(normalize_zero_vector) {
  auto dir = normalize_to_direction(0.0f, 0.0f, 0.0f);
  BOOST_CHECK_CLOSE(dir.x, 0.0f, 1e-5);
  BOOST_CHECK_CLOSE(dir.y, 0.0f, 1e-5);
  BOOST_CHECK_CLOSE(dir.z, 0.0f, 1e-5);
}

BOOST_AUTO_TEST_CASE(normalize_negative) {
  auto dir = normalize_to_direction(-1.0f, 0.0f, 0.0f);
  BOOST_CHECK_CLOSE(dir.x, -1.0f, 1e-5);
  BOOST_CHECK_CLOSE(dir.y, 0.0f, 1e-5);
  BOOST_CHECK_CLOSE(dir.z, 0.0f, 1e-5);
}

BOOST_AUTO_TEST_CASE(normalize_is_unit_length) {
  auto dir = normalize_to_direction(3.0f, 4.0f, 5.0f);
  float mag = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
  BOOST_CHECK_CLOSE(mag, 1.0f, 1e-4);
}

BOOST_AUTO_TEST_CASE(distance_zero) {
  ::caf::SRVector3D a{0.0f, 0.0f, 0.0f};
  ::caf::SRVector3D b{0.0f, 0.0f, 0.0f};
  BOOST_CHECK_CLOSE(distance(a, b), 0.0f, 1e-5);
}

BOOST_AUTO_TEST_CASE(distance_unit_x) {
  ::caf::SRVector3D a{0.0f, 0.0f, 0.0f};
  ::caf::SRVector3D b{1.0f, 0.0f, 0.0f};
  BOOST_CHECK_CLOSE(distance(a, b), 1.0f, 1e-5);
}

BOOST_AUTO_TEST_CASE(distance_unit_y) {
  ::caf::SRVector3D a{0.0f, 0.0f, 0.0f};
  ::caf::SRVector3D b{0.0f, 1.0f, 0.0f};
  BOOST_CHECK_CLOSE(distance(a, b), 1.0f, 1e-5);
}

BOOST_AUTO_TEST_CASE(distance_unit_z) {
  ::caf::SRVector3D a{0.0f, 0.0f, 0.0f};
  ::caf::SRVector3D b{0.0f, 0.0f, 1.0f};
  BOOST_CHECK_CLOSE(distance(a, b), 1.0f, 1e-5);
}

BOOST_AUTO_TEST_CASE(distance_3_4_5) {
  ::caf::SRVector3D a{0.0f, 0.0f, 0.0f};
  ::caf::SRVector3D b{3.0f, 4.0f, 0.0f};
  BOOST_CHECK_CLOSE(distance(a, b), 5.0f, 1e-5);
}

BOOST_AUTO_TEST_CASE(distance_symmetric) {
  ::caf::SRVector3D a{1.0f, 2.0f, 3.0f};
  ::caf::SRVector3D b{4.0f, 5.0f, 6.0f};
  BOOST_CHECK_CLOSE(distance(a, b), distance(b, a), 1e-5);
}

BOOST_AUTO_TEST_CASE(distance_negative_coords) {
  ::caf::SRVector3D a{-1.0f, -2.0f, -3.0f};
  ::caf::SRVector3D b{1.0f, 2.0f, 3.0f};
  float expected = std::sqrt(4.0f + 16.0f + 36.0f);
  BOOST_CHECK_CLOSE(distance(a, b), expected, 1e-4);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// TrueParticleID Creation Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(particle_ids)

BOOST_AUTO_TEST_CASE(make_primary_id_basic) {
  auto id = make_primary_id(42, 0);
  BOOST_TEST(id.ixn == 42);
  BOOST_TEST(id.type == ::caf::TrueParticleID::kPrimary);
  BOOST_TEST(id.part == 0);
}

BOOST_AUTO_TEST_CASE(make_primary_id_different_indices) {
  auto id = make_primary_id(100, 5);
  BOOST_TEST(id.ixn == 100);
  BOOST_TEST(id.type == ::caf::TrueParticleID::kPrimary);
  BOOST_TEST(id.part == 5);
}

BOOST_AUTO_TEST_CASE(make_secondary_id_basic) {
  auto id = make_secondary_id(42, 0);
  BOOST_TEST(id.ixn == 42);
  BOOST_TEST(id.type == ::caf::TrueParticleID::kSecondary);
  BOOST_TEST(id.part == 0);
}

BOOST_AUTO_TEST_CASE(make_secondary_id_different_indices) {
  auto id = make_secondary_id(200, 10);
  BOOST_TEST(id.ixn == 200);
  BOOST_TEST(id.type == ::caf::TrueParticleID::kSecondary);
  BOOST_TEST(id.part == 10);
}

BOOST_AUTO_TEST_CASE(make_prefsi_id_basic) {
  auto id = make_prefsi_id(42, 0);
  BOOST_TEST(id.ixn == 42);
  BOOST_TEST(id.type == ::caf::TrueParticleID::kPrimaryBeforeFSI);
  BOOST_TEST(id.part == 0);
}

BOOST_AUTO_TEST_CASE(make_prefsi_id_different_indices) {
  auto id = make_prefsi_id(300, 15);
  BOOST_TEST(id.ixn == 300);
  BOOST_TEST(id.type == ::caf::TrueParticleID::kPrimaryBeforeFSI);
  BOOST_TEST(id.part == 15);
}

BOOST_AUTO_TEST_CASE(id_types_are_distinct) {
  auto prim = make_primary_id(1, 0);
  auto sec = make_secondary_id(1, 0);
  auto prefsi = make_prefsi_id(1, 0);

  BOOST_TEST(prim.type != sec.type);
  BOOST_TEST(prim.type != prefsi.type);
  BOOST_TEST(sec.type != prefsi.type);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// Truth Match Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(truth_matching)

BOOST_AUTO_TEST_CASE(add_truth_match_to_track) {
  ::caf::SRTrack track{};
  auto id = make_primary_id(1, 0);

  add_truth_match(track, id);

  BOOST_TEST(track.truth.size() == 1);
  BOOST_TEST(track.truthOverlap.size() == 1);
  BOOST_TEST(track.truth[0].ixn == 1);
  BOOST_TEST(track.truth[0].type == ::caf::TrueParticleID::kPrimary);
  BOOST_TEST(track.truth[0].part == 0);
  BOOST_CHECK_CLOSE(track.truthOverlap[0], 1.0f, 1e-5);
}

BOOST_AUTO_TEST_CASE(add_truth_match_to_shower) {
  ::caf::SRShower shower{};
  auto id = make_secondary_id(2, 3);

  add_truth_match(shower, id);

  BOOST_TEST(shower.truth.size() == 1);
  BOOST_TEST(shower.truthOverlap.size() == 1);
  BOOST_TEST(shower.truth[0].ixn == 2);
  BOOST_TEST(shower.truth[0].type == ::caf::TrueParticleID::kSecondary);
  BOOST_TEST(shower.truth[0].part == 3);
}

BOOST_AUTO_TEST_CASE(add_truth_match_to_reco_particle) {
  ::caf::SRRecoParticle reco{};
  auto id = make_prefsi_id(5, 2);

  add_truth_match(reco, id);

  BOOST_TEST(reco.truth.size() == 1);
  BOOST_TEST(reco.truth[0].ixn == 5);
  BOOST_TEST(reco.truth[0].type == ::caf::TrueParticleID::kPrimaryBeforeFSI);
}

BOOST_AUTO_TEST_CASE(add_multiple_truth_matches) {
  ::caf::SRTrack track{};

  add_truth_match(track, make_primary_id(1, 0));
  add_truth_match(track, make_secondary_id(1, 1));
  add_truth_match(track, make_primary_id(2, 0));

  BOOST_TEST(track.truth.size() == 3);
  BOOST_TEST(track.truthOverlap.size() == 3);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// Particle Counter Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(particle_counting)

BOOST_AUTO_TEST_CASE(count_proton) {
  ::caf::SRTrueInteraction ixn{};
  increment_particle_counter(ixn, 2212);
  BOOST_TEST(ixn.nproton == 1);
}

BOOST_AUTO_TEST_CASE(count_multiple_protons) {
  ::caf::SRTrueInteraction ixn{};
  increment_particle_counter(ixn, 2212);
  increment_particle_counter(ixn, 2212);
  increment_particle_counter(ixn, 2212);
  BOOST_TEST(ixn.nproton == 3);
}

BOOST_AUTO_TEST_CASE(count_neutron) {
  ::caf::SRTrueInteraction ixn{};
  increment_particle_counter(ixn, 2112);
  BOOST_TEST(ixn.nneutron == 1);
}

BOOST_AUTO_TEST_CASE(count_pi_plus) {
  ::caf::SRTrueInteraction ixn{};
  increment_particle_counter(ixn, 211);
  BOOST_TEST(ixn.npip == 1);
}

BOOST_AUTO_TEST_CASE(count_pi_minus) {
  ::caf::SRTrueInteraction ixn{};
  increment_particle_counter(ixn, -211);
  BOOST_TEST(ixn.npim == 1);
}

BOOST_AUTO_TEST_CASE(count_pi_zero) {
  ::caf::SRTrueInteraction ixn{};
  increment_particle_counter(ixn, 111);
  BOOST_TEST(ixn.npi0 == 1);
}

BOOST_AUTO_TEST_CASE(count_mixed_particles) {
  ::caf::SRTrueInteraction ixn{};
  increment_particle_counter(ixn, 2212);  // proton
  increment_particle_counter(ixn, 2212);  // proton
  increment_particle_counter(ixn, 2112);  // neutron
  increment_particle_counter(ixn, 211);   // pi+
  increment_particle_counter(ixn, -211);  // pi-
  increment_particle_counter(ixn, 111);   // pi0

  BOOST_TEST(ixn.nproton == 2);
  BOOST_TEST(ixn.nneutron == 1);
  BOOST_TEST(ixn.npip == 1);
  BOOST_TEST(ixn.npim == 1);
  BOOST_TEST(ixn.npi0 == 1);
}

BOOST_AUTO_TEST_CASE(count_unknown_particle_no_crash) {
  ::caf::SRTrueInteraction ixn{};
  // Should not crash, just log debug message
  increment_particle_counter(ixn, 13);    // muon
  increment_particle_counter(ixn, 22);    // gamma
  increment_particle_counter(ixn, 321);   // kaon

  BOOST_TEST(ixn.nproton == 0);
  BOOST_TEST(ixn.nneutron == 0);
}

BOOST_AUTO_TEST_CASE(counters_start_at_zero) {
  ::caf::SRTrueInteraction ixn{};
  BOOST_TEST(ixn.nproton == 0);
  BOOST_TEST(ixn.nneutron == 0);
  BOOST_TEST(ixn.npip == 0);
  BOOST_TEST(ixn.npim == 0);
  BOOST_TEST(ixn.npi0 == 0);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// Constants Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(constants)

BOOST_AUTO_TEST_CASE(nucleon_mass_reasonable) {
  BOOST_CHECK_CLOSE(kNucleonMass_GeV, 0.939f, 1.0);
}

BOOST_AUTO_TEST_CASE(bindino_pdg_value) {
  BOOST_TEST(kBindinoPdg == 2000000101);
}

BOOST_AUTO_TEST_SUITE_END()

FIX_TEST_EXIT
