#define BOOST_TEST_MODULE fast_reco_details_values
#include <boost/test/included/unit_test.hpp>

#include <processes/common/fast_matching/fast_kinematics.hpp>
#include <processes/common/fast_matching/fast_pid.hpp>
#include <processes/common/fast_matching/truth_matching.hpp>

#include <test_helpers.hpp>

#include <duneanaobj/StandardRecord/SRDirectionBranch.h>
#include <duneanaobj/StandardRecord/SREnums.h>
#include <duneanaobj/StandardRecord/SRNeutrinoEnergyBranch.h>
#include <duneanaobj/StandardRecord/SRNeutrinoHypothesisBranch.h>
#include <duneanaobj/StandardRecord/SRRecoParticle.h>
#include <duneanaobj/StandardRecord/SRSAND.h>
#include <duneanaobj/StandardRecord/SRTrueInteraction.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>
#include <duneanaobj/StandardRecord/SRVector3D.h>

namespace {

  using sand::common::direction_from_true;
  using sand::common::energy_from_true;
  using sand::common::match_by_truth;
  using sand::common::neutrino_hypothesis_from_true;
  using sand::common::reco_particle_from_true;

  ::caf::TrueParticleID make_id(int part = 0) { return {0, ::caf::TrueParticleID::kPrimary, part}; }

  double constexpr tol_percent = 1e-4;

  ::caf::TrueParticleID primary(int ixn, int part) { return {ixn, ::caf::TrueParticleID::kPrimary, part}; }
  ::caf::TrueParticleID secondary(int ixn, int part) { return {ixn, ::caf::TrueParticleID::kSecondary, part}; }

  void add_track(::caf::SRSAND& sand, ::caf::TrueParticleID const& id) {
    sand.tracker.tracks.emplace_back().truth = {id};
    sand.tracker.ntracks                     = sand.tracker.tracks.size();
  }

  void add_shower(::caf::SRSAND& sand, ::caf::TrueParticleID const& id) {
    sand.tracker.showers.emplace_back().truth = {id};
    sand.tracker.nshowers                     = sand.tracker.showers.size();
  }

  bool same_particle(::caf::TrueParticleID const& a, ::caf::TrueParticleID const& b) {
    return a.ixn == b.ixn && a.type == b.type && a.part == b.part;
  }

  bool same_object(::caf::SRSANDObjID const& a, ::caf::SRRecoBaseID::SRRecoBaseCollectionType type, int idx) {
    return a.type == type && a.idx == idx;
  }

} // namespace

BOOST_AUTO_TEST_SUITE(neutrino_hypothesis)

BOOST_AUTO_TEST_CASE(cc_numu_one_hot) {
  ::caf::SRTrueInteraction true_ixn;
  true_ixn.pdg      = 14; // numu
  true_ixn.iscc     = true;
  true_ixn.nproton  = 1;
  true_ixn.nneutron = 0;
  true_ixn.npip     = 0;
  true_ixn.npim     = 0;
  true_ixn.npi0     = 0;

  auto const nuhyp = neutrino_hypothesis_from_true(true_ixn);
  auto const& cvn  = nuhyp.cvn;

  BOOST_CHECK_EQUAL(cvn.isnubar, 0.f);
  BOOST_CHECK_EQUAL(cvn.nc, 0.f);
  BOOST_CHECK_EQUAL(cvn.nue, 0.f);
  BOOST_CHECK_EQUAL(cvn.numu, 1.f);
  BOOST_CHECK_EQUAL(cvn.nutau, 0.f);

  BOOST_CHECK_EQUAL(cvn.protons0, 0.f);
  BOOST_CHECK_EQUAL(cvn.protons1, 1.f); // 1 proton -> bucket 1
  BOOST_CHECK_EQUAL(cvn.protons2, 0.f);
  BOOST_CHECK_EQUAL(cvn.protonsN, 0.f);

  BOOST_CHECK_EQUAL(cvn.chgpi0, 1.f); // npip+npim == 0 -> bucket 0
  BOOST_CHECK_EQUAL(cvn.pizero0, 1.f);
  BOOST_CHECK_EQUAL(cvn.neutron0, 1.f);
}

BOOST_AUTO_TEST_CASE(nc_nubar_one_hot) {
  ::caf::SRTrueInteraction true_ixn;
  true_ixn.pdg  = -12; // nue-bar
  true_ixn.iscc = false;

  auto const nuhyp = neutrino_hypothesis_from_true(true_ixn);
  auto const& cvn  = nuhyp.cvn;

  BOOST_CHECK_EQUAL(cvn.isnubar, 1.f); // pdg < 0
  BOOST_CHECK_EQUAL(cvn.nc, 1.f);      // iscc == false
  // NC: none of nue/numu/nutau should fire, regardless of the probe pdg
  BOOST_CHECK_EQUAL(cvn.nue, 0.f);
  BOOST_CHECK_EQUAL(cvn.numu, 0.f);
  BOOST_CHECK_EQUAL(cvn.nutau, 0.f);
}

BOOST_AUTO_TEST_CASE(count_bucket_saturates_at_n_geq_3) {
  ::caf::SRTrueInteraction true_ixn;
  true_ixn.pdg     = 14;
  true_ixn.iscc    = true;
  true_ixn.nproton = 5; // >= 3 -> saturates in the "N" bucket, not out of bounds

  auto const nuhyp = neutrino_hypothesis_from_true(true_ixn);
  auto const& cvn  = nuhyp.cvn;

  BOOST_CHECK_EQUAL(cvn.protons0, 0.f);
  BOOST_CHECK_EQUAL(cvn.protons1, 0.f);
  BOOST_CHECK_EQUAL(cvn.protons2, 0.f);
  BOOST_CHECK_EQUAL(cvn.protonsN, 1.f);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(direction_and_energy)

BOOST_AUTO_TEST_CASE(direction_is_normalized_and_replicated) {
  ::caf::SRTrueInteraction true_ixn;
  true_ixn.momentum = ::caf::SRVector3D{3.f, 4.f, 0.f}; // |.| == 5

  auto const dir = direction_from_true(true_ixn);

  for (auto const* v : {&dir.calo, &dir.heshw, &dir.lngtrk, &dir.part_mom_sum}) {
    BOOST_CHECK_CLOSE(v->x, 0.6f, tol_percent);
    BOOST_CHECK_CLOSE(v->y, 0.8f, tol_percent);
    BOOST_CHECK_SMALL(v->z, 1e-6f);
  }
}

BOOST_AUTO_TEST_CASE(direction_zero_momentum_is_zero_not_nan) {
  ::caf::SRTrueInteraction true_ixn; // momentum defaults to (0,0,0)

  auto const dir = direction_from_true(true_ixn);

  BOOST_CHECK_EQUAL(dir.calo.x, 0.f);
  BOOST_CHECK_EQUAL(dir.calo.y, 0.f);
  BOOST_CHECK_EQUAL(dir.calo.z, 0.f);
}

BOOST_AUTO_TEST_CASE(energy_collapses_to_true_value) {
  ::caf::SRTrueInteraction true_ixn;
  true_ixn.E = 2.5f;

  auto const enu = energy_from_true(true_ixn);

  for (float const v : {enu.calo, enu.lep_calo, enu.mu_range, enu.mu_mcs, enu.mu_mcs_llhd, enu.e_calo, enu.e_had,
                        enu.mu_had, enu.regcnn, enu.part_energy_sum}) {
    BOOST_CHECK_EQUAL(v, 2.5f);
  }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(gev_not_mev)

BOOST_AUTO_TEST_CASE(reco_particle_energy_and_momentum_stay_in_gev) {
  ::caf::SRTrueParticle true_part;
  true_part.pdg       = 13;
  true_part.p.px      = 0.3f;
  true_part.p.py      = 0.4f;
  true_part.p.pz      = 0.f;
  true_part.p.E       = 1.2f; // GeV
  true_part.start_pos = ::caf::SRVector3D{1.f, 2.f, 3.f};
  true_part.end_pos   = ::caf::SRVector3D{4.f, 5.f, 6.f};

  auto const id = make_id();
  auto const rp = reco_particle_from_true(true_part, id);

  BOOST_CHECK(rp.primary);
  BOOST_CHECK_EQUAL(rp.pdg, 13);
  BOOST_CHECK_EQUAL(rp.score, 1.f);
  BOOST_CHECK_CLOSE(rp.E, 1.2f, tol_percent); // GeV, not 1200.f
  BOOST_CHECK_CLOSE(rp.p.x, 0.3f, tol_percent);
  BOOST_CHECK_CLOSE(rp.p.y, 0.4f, tol_percent);
  BOOST_CHECK_CLOSE(rp.p.z, 0.f, tol_percent);
  BOOST_CHECK_EQUAL(rp.start.x, 1.f);
  BOOST_CHECK_EQUAL(rp.end.x, 4.f);
  BOOST_REQUIRE_EQUAL(rp.truth.size(), 1u);
  BOOST_CHECK_EQUAL(rp.truthOverlap.at(0), 1.f);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(matching)

BOOST_AUTO_TEST_CASE(no_objects_no_matches) {
  ::caf::SRSAND const sand;

  BOOST_CHECK(match_by_truth(sand).empty());
}

BOOST_AUTO_TEST_CASE(objects_of_the_same_particle_are_one_match) {
  ::caf::SRSAND sand;
  add_shower(sand, primary(0, 2));
  add_track(sand, primary(0, 2));

  auto const matches = match_by_truth(sand);

  BOOST_REQUIRE_EQUAL(matches.size(), 1u);
  BOOST_CHECK(same_particle(matches[0].trk.truth.at(0), primary(0, 2)));
  // within a particle, tracks come before showers: the order the collections are scanned in
  BOOST_REQUIRE_EQUAL(matches[0].constituents.size(), 2u);
  BOOST_CHECK(same_object(matches[0].constituents[0], ::caf::SRRecoBaseID::kSANDTrackerTrack, 0));
  BOOST_CHECK(same_object(matches[0].constituents[1], ::caf::SRRecoBaseID::kSANDTrackerShower, 0));
}

BOOST_AUTO_TEST_CASE(same_index_in_another_collection_is_another_particle) {
  ::caf::SRSAND sand;
  add_track(sand, primary(0, 0));
  add_track(sand, secondary(0, 0)); // same part, other collection
  add_track(sand, primary(1, 0));   // same part, other interaction

  BOOST_CHECK_EQUAL(match_by_truth(sand).size(), 3u);
}

BOOST_AUTO_TEST_CASE(matches_are_ordered_by_particle) {
  ::caf::SRSAND sand;
  add_track(sand, primary(1, 0));   // track 0
  add_track(sand, secondary(0, 0)); // track 1
  add_shower(sand, primary(0, 1));  // shower 0
  add_track(sand, primary(0, 0));   // track 2

  auto const matches = match_by_truth(sand);

  // interaction first, then primaries before secondaries, then position
  BOOST_REQUIRE_EQUAL(matches.size(), 4u);
  BOOST_CHECK(same_particle(matches[0].trk.truth.at(0), primary(0, 0)));
  BOOST_CHECK(same_particle(matches[1].trk.truth.at(0), primary(0, 1)));
  BOOST_CHECK(same_particle(matches[2].trk.truth.at(0), secondary(0, 0)));
  BOOST_CHECK(same_particle(matches[3].trk.truth.at(0), primary(1, 0)));

  // every constituent still names its own object
  BOOST_CHECK(same_object(matches[0].constituents.at(0), ::caf::SRRecoBaseID::kSANDTrackerTrack, 2));
  BOOST_CHECK(same_object(matches[1].constituents.at(0), ::caf::SRRecoBaseID::kSANDTrackerShower, 0));
  BOOST_CHECK(same_object(matches[2].constituents.at(0), ::caf::SRRecoBaseID::kSANDTrackerTrack, 1));
  BOOST_CHECK(same_object(matches[3].constituents.at(0), ::caf::SRRecoBaseID::kSANDTrackerTrack, 0));
}

BOOST_AUTO_TEST_SUITE_END()

FIX_TEST_EXIT
