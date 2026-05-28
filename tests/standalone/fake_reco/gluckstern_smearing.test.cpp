#define BOOST_TEST_MODULE caf_filler
#include <boost/test/included/unit_test.hpp>

#include <processes/fake_reco/caf_handlers/caf_filler.hpp>
#include <processes/fake_reco/caf_handlers/caf_filler_common.hpp>
#include <processes/fake_reco/utils/GlucksternSmearing.hpp>
#include <test_helpers.hpp>
#include <TG4Event.h>
#include <data/common/edep_reader/EDEPTrajectory.h>

#include <cmath>
#include <iostream>

using namespace sand;

// ====================================================================================
// Helper: Create test SRTrueParticle, SRTrueInteraction, and EDEPTrajectory with hits
// ====================================================================================

namespace {
  void set_vector(::caf::SRVector3D& v, float x, float y, float z) {
    v.x = x;
    v.y = y;
    v.z = z;
  }

  ::caf::SRTrueParticle make_test_true_particle(int pdg, float px, float py, float pz, float E) {
    ::caf::SRTrueParticle p{};
    p.pdg  = pdg;
    p.p.px = px;
    p.p.py = py;
    p.p.pz = pz;
    p.p.E  = E;
    set_vector(p.start_pos, 0.0f, 0.0f, 0.0f);
    set_vector(p.end_pos, 10.0f, 20.0f, 30.0f);
    p.time = 0.0f;
    return p;
  }

  ::caf::SRTrueInteraction make_test_true_interaction() {
    ::caf::SRTrueInteraction ixn{};
    ixn.id = 42;
    ixn.E  = 2.5f;
    set_vector(ixn.vtx, 100.0f, 200.0f, 300.0f);
    return ixn;
  }

  // create a test hit, where
  // contrib: The main contributor of the hit.
  // primary_id: The ID of the primary particle generating the hit.
  // i: The index of the hit.
  EDEPHit make_test_hit(const vec_4d& start, const vec_4d& stop, double energy_deposit, int contrib, int primary_id,
                        int i) {
    double sec_dep      = 50.0f;
    double track_length = 30.0f;

    auto hit = EDEPHit(start, stop, energy_deposit, sec_dep, track_length, contrib, primary_id, i);
    return hit;
  }

  // create a test EDEPTrajectory, starting from the default TG4Trajectory
  // and filling it with a specified nr of test hits and default TG4 primary
  EDEPTrajectory make_test_true_trajectory(int nr_hit_points, double energy_deposit, int contrib, int primary_id) {
    auto traj = TG4Trajectory();

    // get the id of the default TG4Trajectory
    int trackId = traj.GetTrackId();

    std::vector<EDEPHit> hit_vec;
    float z = 0.0f;
    for (int i{}; i < nr_hit_points; ++i) {
      vec_4d start{0.0f, 0.0f, z, 0.0f};
      vec_4d stop{0.0f, 0.0f, z + 2.0f, 0.0f};
      hit_vec.emplace_back(make_test_hit(start, stop, energy_deposit, contrib, primary_id, i));
      z += 2.0f;
    }

    // hit map with test hits
    std::map<int, std::map<component, std::vector<EDEPHit>>> hit_map;
    hit_map[trackId][component::DRIFT] = hit_vec;

    // default TG4 primaries (1 primary)
    TG4PrimaryVertexContainer primaries;
    for (int i{}; i < 1; ++i) {
      primaries.push_back(TG4PrimaryVertex());
    }

    auto edep_traj = EDEPTrajectory(traj, hit_map, primaries);

    return edep_traj;
  }

} // namespace

// =============================================================================
// CAFFiller<SRRecoParticle>::from_true_with_mu_smearing Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(reco_particle_filler_with_smearing)

BOOST_AUTO_TEST_CASE(generic_particle_not_smeared) {
  auto true_part     = make_test_true_particle(2212, 0.0f, 0.0f, 1.0f, 1.1f);
  auto id            = make_primary_id(1, 0);
  auto true_part_trj = make_test_true_trajectory(5, 100.0f, 2212, 2212);

  auto reco = CAFFiller<::caf::SRRecoParticle>::from_true_with_mu_smearing(true_part, id, true_part_trj, 0.1f, 0.1f,
                                                                           50.0f, 1.0f);

  BOOST_TEST(reco.pdg == 2212);
  BOOST_TEST(reco.primary == true);
  BOOST_CHECK_CLOSE(reco.E, 1.1f, 1e-5);
  BOOST_TEST(reco.E_method == ::caf::PartEMethod::kCalorimetry);
  BOOST_CHECK_CLOSE(reco.p.x, 0.0f, 1e-5);
  BOOST_CHECK_CLOSE(reco.p.y, 0.0f, 1e-5);
  BOOST_CHECK_CLOSE(reco.p.z, 1.0f, 1e-5);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// Edge Cases
// =============================================================================

BOOST_AUTO_TEST_SUITE(edge_cases)

BOOST_AUTO_TEST_CASE(few_hit_points) {
  auto true_part     = make_test_true_particle(13, 0.0f, 0.0f, 1.0f, 1.1f);
  auto id            = make_primary_id(1, 0);
  auto true_part_trj = make_test_true_trajectory(1, 100.0f, 13, 13);

  const auto& hit_map = true_part_trj.GetHitMap();
  const auto& it      = hit_map.find(component::DRIFT);
  const auto& hit_vec = it->second;
  BOOST_TEST_MESSAGE("hit_vec.size() = " << hit_vec.size());

  const auto gluckstern_helper = GlucksternSmearing(hit_vec, 50.0f);
  auto reco = CAFFiller<::caf::SRRecoParticle>::from_true_with_mu_smearing(true_part, id, true_part_trj, 0.1f, 0.1f,
                                                                           50.0f, 1.0f);

  BOOST_TEST(!gluckstern_helper.IsValid());
  BOOST_TEST(reco.pdg == 13);
  BOOST_TEST(reco.primary == true);
  BOOST_CHECK_CLOSE(reco.E, 1.1f, 1e-5);
  BOOST_TEST(reco.E_method == ::caf::PartEMethod::kCalorimetry);
  BOOST_CHECK_CLOSE(reco.p.x, 0.0f, 1e-5);
  BOOST_CHECK_CLOSE(reco.p.y, 0.0f, 1e-5);
  BOOST_CHECK_CLOSE(reco.p.z, 1.0f, 1e-5);
}

BOOST_AUTO_TEST_CASE(points_over_thr) {
  auto true_part_trj = make_test_true_trajectory(3, 100.0f, 13, 13);

  const auto& hit_map          = true_part_trj.GetHitMap();
  const auto& it               = hit_map.find(component::DRIFT);
  const auto& hit_vec          = it->second;
  const auto gluckstern_helper = GlucksternSmearing(hit_vec, 50.0f);

  int expect_hits_over_thr = 3;
  BOOST_TEST(gluckstern_helper.GetNHits() == expect_hits_over_thr);
}

BOOST_AUTO_TEST_SUITE_END()

FIX_TEST_EXIT
