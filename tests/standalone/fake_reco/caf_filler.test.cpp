#define BOOST_TEST_MODULE caf_filler
#include <boost/test/included/unit_test.hpp>

#include <processes/fake_reco/caf_handlers/caf_filler.hpp>
#include <processes/fake_reco/caf_handlers/caf_filler_common.hpp>
#include <test_helpers.hpp>

#include <cmath>

using namespace sand;

// =============================================================================
// Helper: Create test SRTrueParticle
// =============================================================================

namespace {
  void set_vector(::caf::SRVector3D& v, float x, float y, float z) {
    v.x = x;
    v.y = y;
    v.z = z;
  }

  ::caf::SRTrueParticle make_test_true_particle(int pdg, float px, float py, float pz, float E) {
    ::caf::SRTrueParticle p{};
    p.pdg = pdg;
    p.p.px = px;
    p.p.py = py;
    p.p.pz = pz;
    p.p.E = E;
    set_vector(p.start_pos, 0.0f, 0.0f, 0.0f);
    set_vector(p.end_pos, 10.0f, 20.0f, 30.0f);
    p.time = 0.0f;
    return p;
  }

  ::caf::SRTrueInteraction make_test_true_interaction() {
    ::caf::SRTrueInteraction ixn{};
    ixn.id = 42;
    ixn.E = 2.5f;
    set_vector(ixn.vtx, 100.0f, 200.0f, 300.0f);
    return ixn;
  }
}

// =============================================================================
// CAFFiller<SRRecoParticle>::from_true Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(reco_particle_filler)

BOOST_AUTO_TEST_CASE(from_true_muon) {
  auto true_part = make_test_true_particle(13, 0.0f, 0.0f, 1.0f, 1.1f);
  auto id = make_primary_id(1, 0);

  auto reco = CAFFiller<::caf::SRRecoParticle>::from_true(true_part, id);

  BOOST_TEST(reco.pdg == 13);
  BOOST_TEST(reco.primary == true);
  BOOST_CHECK_CLOSE(reco.E, 1.1f, 1e-5);
  BOOST_TEST(reco.E_method == ::caf::PartEMethod::kCalorimetry);
  BOOST_CHECK_CLOSE(reco.p.x, 0.0f, 1e-5);
  BOOST_CHECK_CLOSE(reco.p.y, 0.0f, 1e-5);
  BOOST_CHECK_CLOSE(reco.p.z, 1.0f, 1e-5);
}

BOOST_AUTO_TEST_CASE(from_true_track_like) {
  auto true_part = make_test_true_particle(13, 0.0f, 0.0f, 1.0f, 1.0f);  // muon
  auto id = make_primary_id(1, 0);

  auto reco = CAFFiller<::caf::SRRecoParticle>::from_true(true_part, id);

  BOOST_TEST(reco.origRecoObjType == ::caf::RecoObjType::kTrack);
}

BOOST_AUTO_TEST_CASE(from_true_proton_is_track) {
  auto true_part = make_test_true_particle(2212, 0.5f, 0.0f, 0.5f, 1.0f);
  auto id = make_primary_id(1, 0);

  auto reco = CAFFiller<::caf::SRRecoParticle>::from_true(true_part, id);

  BOOST_TEST(reco.origRecoObjType == ::caf::RecoObjType::kTrack);
}

BOOST_AUTO_TEST_CASE(from_true_pion_plus_is_track) {
  auto true_part = make_test_true_particle(211, 0.1f, 0.2f, 0.3f, 0.5f);
  auto id = make_primary_id(1, 0);

  auto reco = CAFFiller<::caf::SRRecoParticle>::from_true(true_part, id);

  BOOST_TEST(reco.origRecoObjType == ::caf::RecoObjType::kTrack);
}

BOOST_AUTO_TEST_CASE(from_true_shower_like_electron) {
  auto true_part = make_test_true_particle(11, 0.0f, 0.0f, 0.5f, 0.5f);  // electron
  auto id = make_primary_id(1, 0);

  auto reco = CAFFiller<::caf::SRRecoParticle>::from_true(true_part, id);

  BOOST_TEST(reco.origRecoObjType == ::caf::RecoObjType::kShower);
}

BOOST_AUTO_TEST_CASE(from_true_shower_like_gamma) {
  auto true_part = make_test_true_particle(22, 0.0f, 0.0f, 1.0f, 1.0f);  // gamma
  auto id = make_primary_id(1, 0);

  auto reco = CAFFiller<::caf::SRRecoParticle>::from_true(true_part, id);

  BOOST_TEST(reco.origRecoObjType == ::caf::RecoObjType::kShower);
}

BOOST_AUTO_TEST_CASE(from_true_shower_like_pi0) {
  auto true_part = make_test_true_particle(111, 0.1f, 0.1f, 0.1f, 0.3f);  // pi0
  auto id = make_primary_id(1, 0);

  auto reco = CAFFiller<::caf::SRRecoParticle>::from_true(true_part, id);

  BOOST_TEST(reco.origRecoObjType == ::caf::RecoObjType::kShower);
}

BOOST_AUTO_TEST_CASE(from_true_score_is_one) {
  auto true_part = make_test_true_particle(13, 0.0f, 0.0f, 1.0f, 1.0f);
  auto id = make_primary_id(1, 0);

  auto reco = CAFFiller<::caf::SRRecoParticle>::from_true(true_part, id);

  BOOST_CHECK_CLOSE(reco.score, 1.0f, 1e-5);
}

BOOST_AUTO_TEST_CASE(from_true_positions_copied) {
  auto true_part = make_test_true_particle(13, 0.0f, 0.0f, 1.0f, 1.0f);
  set_vector(true_part.start_pos, 1.0f, 2.0f, 3.0f);
  set_vector(true_part.end_pos, 4.0f, 5.0f, 6.0f);
  auto id = make_primary_id(1, 0);

  auto reco = CAFFiller<::caf::SRRecoParticle>::from_true(true_part, id);

  BOOST_CHECK_CLOSE(reco.start.x, 1.0f, 1e-5);
  BOOST_CHECK_CLOSE(reco.start.y, 2.0f, 1e-5);
  BOOST_CHECK_CLOSE(reco.start.z, 3.0f, 1e-5);
  BOOST_CHECK_CLOSE(reco.end.x, 4.0f, 1e-5);
  BOOST_CHECK_CLOSE(reco.end.y, 5.0f, 1e-5);
  BOOST_CHECK_CLOSE(reco.end.z, 6.0f, 1e-5);
}

BOOST_AUTO_TEST_CASE(from_true_has_truth_match) {
  auto true_part = make_test_true_particle(13, 0.0f, 0.0f, 1.0f, 1.0f);
  auto id = make_primary_id(42, 3);

  auto reco = CAFFiller<::caf::SRRecoParticle>::from_true(true_part, id);

  BOOST_TEST(reco.truth.size() == 1);
  BOOST_TEST(reco.truthOverlap.size() == 1);
  BOOST_TEST(reco.truth[0].ixn == 42);
  BOOST_TEST(reco.truth[0].part == 3);
  BOOST_CHECK_CLOSE(reco.truthOverlap[0], 1.0f, 1e-5);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// CAFFiller<SRTrack>::from_true Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(track_filler)

BOOST_AUTO_TEST_CASE(from_true_positions) {
  auto true_part = make_test_true_particle(13, 0.0f, 0.0f, 1.0f, 1.0f);
  set_vector(true_part.start_pos, 10.0f, 20.0f, 30.0f);
  set_vector(true_part.end_pos, 40.0f, 50.0f, 60.0f);
  auto id = make_primary_id(1, 0);

  auto track = CAFFiller<::caf::SRTrack>::from_true(true_part, id);

  BOOST_CHECK_CLOSE(track.start.x, 10.0f, 1e-5);
  BOOST_CHECK_CLOSE(track.start.y, 20.0f, 1e-5);
  BOOST_CHECK_CLOSE(track.start.z, 30.0f, 1e-5);
  BOOST_CHECK_CLOSE(track.end.x, 40.0f, 1e-5);
  BOOST_CHECK_CLOSE(track.end.y, 50.0f, 1e-5);
  BOOST_CHECK_CLOSE(track.end.z, 60.0f, 1e-5);
}

BOOST_AUTO_TEST_CASE(from_true_direction_normalized) {
  auto true_part = make_test_true_particle(13, 3.0f, 4.0f, 0.0f, 5.0f);
  auto id = make_primary_id(1, 0);

  auto track = CAFFiller<::caf::SRTrack>::from_true(true_part, id);

  BOOST_CHECK_CLOSE(track.dir.x, 0.6f, 1e-4);
  BOOST_CHECK_CLOSE(track.dir.y, 0.8f, 1e-4);
  BOOST_CHECK_CLOSE(track.dir.z, 0.0f, 1e-5);

  float mag = std::sqrt(track.dir.x * track.dir.x + track.dir.y * track.dir.y + track.dir.z * track.dir.z);
  BOOST_CHECK_CLOSE(mag, 1.0f, 1e-4);
}

BOOST_AUTO_TEST_CASE(from_true_enddir_equals_dir) {
  auto true_part = make_test_true_particle(13, 1.0f, 0.0f, 0.0f, 1.0f);
  auto id = make_primary_id(1, 0);

  auto track = CAFFiller<::caf::SRTrack>::from_true(true_part, id);

  BOOST_CHECK_CLOSE(track.enddir.x, track.dir.x, 1e-5);
  BOOST_CHECK_CLOSE(track.enddir.y, track.dir.y, 1e-5);
  BOOST_CHECK_CLOSE(track.enddir.z, track.dir.z, 1e-5);
}

BOOST_AUTO_TEST_CASE(from_true_time) {
  auto true_part = make_test_true_particle(13, 0.0f, 0.0f, 1.0f, 1.0f);
  true_part.time = 123.45f;
  auto id = make_primary_id(1, 0);

  auto track = CAFFiller<::caf::SRTrack>::from_true(true_part, id);

  BOOST_CHECK_CLOSE(track.time, 123.45f, 1e-4);
}

BOOST_AUTO_TEST_CASE(from_true_energy_in_mev) {
  auto true_part = make_test_true_particle(13, 0.0f, 0.0f, 1.0f, 2.5f);  // 2.5 GeV
  auto id = make_primary_id(1, 0);

  auto track = CAFFiller<::caf::SRTrack>::from_true(true_part, id);

  BOOST_CHECK_CLOSE(track.E, 2500.0f, 1e-3);  // 2500 MeV
  BOOST_CHECK_CLOSE(track.Evis, 2500.0f, 1e-3);
}

BOOST_AUTO_TEST_CASE(from_true_length) {
  auto true_part = make_test_true_particle(13, 0.0f, 0.0f, 1.0f, 1.0f);
  set_vector(true_part.start_pos, 0.0f, 0.0f, 0.0f);
  set_vector(true_part.end_pos, 3.0f, 4.0f, 0.0f);
  auto id = make_primary_id(1, 0);

  auto track = CAFFiller<::caf::SRTrack>::from_true(true_part, id);

  BOOST_CHECK_CLOSE(track.len_cm, 5.0f, 1e-5);
}

BOOST_AUTO_TEST_CASE(from_true_charge_muon_minus) {
  auto true_part = make_test_true_particle(13, 0.0f, 0.0f, 1.0f, 1.0f);  // mu-
  auto id = make_primary_id(1, 0);

  auto track = CAFFiller<::caf::SRTrack>::from_true(true_part, id);

  BOOST_TEST(track.charge == -1);
}

BOOST_AUTO_TEST_CASE(from_true_charge_muon_plus) {
  auto true_part = make_test_true_particle(-13, 0.0f, 0.0f, 1.0f, 1.0f);  // mu+
  auto id = make_primary_id(1, 0);

  auto track = CAFFiller<::caf::SRTrack>::from_true(true_part, id);

  BOOST_TEST(track.charge == +1);
}

BOOST_AUTO_TEST_CASE(from_true_charge_proton) {
  auto true_part = make_test_true_particle(2212, 0.0f, 0.0f, 1.0f, 1.0f);
  auto id = make_primary_id(1, 0);

  auto track = CAFFiller<::caf::SRTrack>::from_true(true_part, id);

  BOOST_TEST(track.charge == +1);
}

BOOST_AUTO_TEST_CASE(from_true_quality_one) {
  auto true_part = make_test_true_particle(13, 0.0f, 0.0f, 1.0f, 1.0f);
  auto id = make_primary_id(1, 0);

  auto track = CAFFiller<::caf::SRTrack>::from_true(true_part, id);

  BOOST_CHECK_CLOSE(track.qual, 1.0f, 1e-5);
}

BOOST_AUTO_TEST_CASE(from_true_has_truth_match) {
  auto true_part = make_test_true_particle(13, 0.0f, 0.0f, 1.0f, 1.0f);
  auto id = make_primary_id(99, 7);

  auto track = CAFFiller<::caf::SRTrack>::from_true(true_part, id);

  BOOST_TEST(track.truth.size() == 1);
  BOOST_TEST(track.truth[0].ixn == 99);
  BOOST_TEST(track.truth[0].part == 7);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// CAFFiller<SRShower>::from_true Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(shower_filler)

BOOST_AUTO_TEST_CASE(from_true_start_position) {
  auto true_part = make_test_true_particle(11, 0.0f, 0.0f, 1.0f, 0.5f);
  set_vector(true_part.start_pos, 5.0f, 10.0f, 15.0f);
  auto id = make_primary_id(1, 0);

  auto shower = CAFFiller<::caf::SRShower>::from_true(true_part, id);

  BOOST_CHECK_CLOSE(shower.start.x, 5.0f, 1e-5);
  BOOST_CHECK_CLOSE(shower.start.y, 10.0f, 1e-5);
  BOOST_CHECK_CLOSE(shower.start.z, 15.0f, 1e-5);
}

BOOST_AUTO_TEST_CASE(from_true_direction_normalized) {
  auto true_part = make_test_true_particle(11, 1.0f, 2.0f, 2.0f, 0.5f);
  auto id = make_primary_id(1, 0);

  auto shower = CAFFiller<::caf::SRShower>::from_true(true_part, id);

  float mag = std::sqrt(shower.direction.x * shower.direction.x +
                        shower.direction.y * shower.direction.y +
                        shower.direction.z * shower.direction.z);
  BOOST_CHECK_CLOSE(mag, 1.0f, 1e-4);
}

BOOST_AUTO_TEST_CASE(from_true_direction_values) {
  auto true_part = make_test_true_particle(11, 0.0f, 0.0f, 5.0f, 5.0f);
  auto id = make_primary_id(1, 0);

  auto shower = CAFFiller<::caf::SRShower>::from_true(true_part, id);

  BOOST_CHECK_CLOSE(shower.direction.x, 0.0f, 1e-5);
  BOOST_CHECK_CLOSE(shower.direction.y, 0.0f, 1e-5);
  BOOST_CHECK_CLOSE(shower.direction.z, 1.0f, 1e-5);
}

BOOST_AUTO_TEST_CASE(from_true_energy_in_mev) {
  auto true_part = make_test_true_particle(11, 0.0f, 0.0f, 1.0f, 1.5f);  // 1.5 GeV
  auto id = make_primary_id(1, 0);

  auto shower = CAFFiller<::caf::SRShower>::from_true(true_part, id);

  BOOST_CHECK_CLOSE(shower.Evis, 1500.0f, 1e-3);  // 1500 MeV
}

BOOST_AUTO_TEST_CASE(from_true_has_truth_match) {
  auto true_part = make_test_true_particle(11, 0.0f, 0.0f, 1.0f, 0.5f);
  auto id = make_secondary_id(50, 2);

  auto shower = CAFFiller<::caf::SRShower>::from_true(true_part, id);

  BOOST_TEST(shower.truth.size() == 1);
  BOOST_TEST(shower.truth[0].ixn == 50);
  BOOST_TEST(shower.truth[0].type == ::caf::TrueParticleID::kSecondary);
  BOOST_TEST(shower.truth[0].part == 2);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// CAFFiller<SRInteraction>::from_true Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(interaction_filler)

BOOST_AUTO_TEST_CASE(from_true_id) {
  auto true_ixn = make_test_true_interaction();
  true_ixn.id = 123;

  auto reco = CAFFiller<::caf::SRInteraction>::from_true(true_ixn, 0);

  BOOST_TEST(reco.id == 123);
}

BOOST_AUTO_TEST_CASE(from_true_vertex) {
  auto true_ixn = make_test_true_interaction();
  set_vector(true_ixn.vtx, 10.0f, 20.0f, 30.0f);

  auto reco = CAFFiller<::caf::SRInteraction>::from_true(true_ixn, 0);

  BOOST_CHECK_CLOSE(reco.vtx.x, 10.0f, 1e-5);
  BOOST_CHECK_CLOSE(reco.vtx.y, 20.0f, 1e-5);
  BOOST_CHECK_CLOSE(reco.vtx.z, 30.0f, 1e-5);
}

BOOST_AUTO_TEST_CASE(from_true_energy) {
  auto true_ixn = make_test_true_interaction();
  true_ixn.E = 3.5f;

  auto reco = CAFFiller<::caf::SRInteraction>::from_true(true_ixn, 0);

  BOOST_CHECK_CLOSE(reco.Enu.calo, 3.5f, 1e-5);
}

BOOST_AUTO_TEST_CASE(from_true_truth_index) {
  auto true_ixn = make_test_true_interaction();

  auto reco = CAFFiller<::caf::SRInteraction>::from_true(true_ixn, 5);

  BOOST_TEST(reco.truth.size() == 1);
  BOOST_TEST(reco.truth[0] == 5);
}

BOOST_AUTO_TEST_CASE(from_true_truth_overlap) {
  auto true_ixn = make_test_true_interaction();

  auto reco = CAFFiller<::caf::SRInteraction>::from_true(true_ixn, 0);

  BOOST_TEST(reco.truthOverlap.size() == 1);
  BOOST_CHECK_CLOSE(reco.truthOverlap[0], 1.0f, 1e-5);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// Edge Cases
// =============================================================================

BOOST_AUTO_TEST_SUITE(edge_cases)

BOOST_AUTO_TEST_CASE(zero_momentum_track) {
  auto true_part = make_test_true_particle(13, 0.0f, 0.0f, 0.0f, 0.1f);
  auto id = make_primary_id(1, 0);

  auto track = CAFFiller<::caf::SRTrack>::from_true(true_part, id);

  BOOST_CHECK_CLOSE(track.dir.x, 0.0f, 1e-5);
  BOOST_CHECK_CLOSE(track.dir.y, 0.0f, 1e-5);
  BOOST_CHECK_CLOSE(track.dir.z, 0.0f, 1e-5);
}

BOOST_AUTO_TEST_CASE(zero_momentum_shower) {
  auto true_part = make_test_true_particle(11, 0.0f, 0.0f, 0.0f, 0.0005f);
  auto id = make_primary_id(1, 0);

  auto shower = CAFFiller<::caf::SRShower>::from_true(true_part, id);

  BOOST_CHECK_CLOSE(shower.direction.x, 0.0f, 1e-5);
  BOOST_CHECK_CLOSE(shower.direction.y, 0.0f, 1e-5);
  BOOST_CHECK_CLOSE(shower.direction.z, 0.0f, 1e-5);
}

BOOST_AUTO_TEST_CASE(very_small_momentum) {
  auto true_part = make_test_true_particle(13, 1e-10f, 0.0f, 0.0f, 0.1f);
  auto id = make_primary_id(1, 0);

  auto track = CAFFiller<::caf::SRTrack>::from_true(true_part, id);

  // Should normalize to unit vector in x direction
  BOOST_CHECK_CLOSE(track.dir.x, 1.0f, 1e-4);
}

BOOST_AUTO_TEST_CASE(large_energy) {
  auto true_part = make_test_true_particle(13, 0.0f, 0.0f, 100.0f, 100.0f);  // 100 GeV
  auto id = make_primary_id(1, 0);

  auto track = CAFFiller<::caf::SRTrack>::from_true(true_part, id);

  BOOST_CHECK_CLOSE(track.E, 100000.0f, 1e-3);  // 100 TeV in MeV
}

BOOST_AUTO_TEST_CASE(negative_coordinates) {
  auto true_part = make_test_true_particle(13, -1.0f, -2.0f, -3.0f, 4.0f);
  set_vector(true_part.start_pos, -100.0f, -200.0f, -300.0f);
  set_vector(true_part.end_pos, -50.0f, -100.0f, -150.0f);
  auto id = make_primary_id(1, 0);

  auto track = CAFFiller<::caf::SRTrack>::from_true(true_part, id);

  BOOST_CHECK_CLOSE(track.start.x, -100.0f, 1e-5);
  BOOST_CHECK_CLOSE(track.end.x, -50.0f, 1e-5);

  // Length should be positive
  BOOST_TEST(track.len_cm > 0.0f);
}

BOOST_AUTO_TEST_SUITE_END()

FIX_TEST_EXIT
