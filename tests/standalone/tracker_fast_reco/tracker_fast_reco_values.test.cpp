#define BOOST_TEST_MODULE tracker_fast_reco_values
#include <boost/test/included/unit_test.hpp>

#include <processes/tracker/fast_reco/fast_reco.hpp>

#include <test_helpers.hpp>

#include <duneanaobj/StandardRecord/SREnums.h>
#include <duneanaobj/StandardRecord/SRShower.h>
#include <duneanaobj/StandardRecord/SRTrack.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>
#include <duneanaobj/StandardRecord/SRVector3D.h>

namespace {

  using sand::tracker::shower_from_true;
  using sand::tracker::track_from_true;

  ::caf::TrueParticleID make_id(int part = 0) { return {0, ::caf::TrueParticleID::kPrimary, part}; }

  double constexpr tol_percent = 1e-4;

} // namespace

BOOST_AUTO_TEST_SUITE(mev_not_gev)

BOOST_AUTO_TEST_CASE(track_energy_direction_charge_and_length) {
  ::caf::SRTrueParticle true_part;
  true_part.pdg       = 13; // mu-
  true_part.p.px      = 0.3f;
  true_part.p.py      = 0.4f;
  true_part.p.pz      = 0.f;
  true_part.p.E       = 1.2f; // GeV
  true_part.time      = 12.5;
  true_part.start_pos = ::caf::SRVector3D{0.f, 0.f, 0.f};
  true_part.end_pos   = ::caf::SRVector3D{3.f, 4.f, 0.f}; // distance == 5 cm (3-4-5)

  auto const track = track_from_true(true_part, make_id(), nullptr); // no trajectory -> enddir == start direction

  BOOST_CHECK_CLOSE(track.E, 1200.f, tol_percent); // MeV, not 1.2f GeV -- the regression this guards
  BOOST_CHECK_CLOSE(track.Evis, track.E, tol_percent);
  BOOST_CHECK_CLOSE(track.dir.x, 0.6f, tol_percent); // normalize_to_direction(0.3, 0.4, 0)
  BOOST_CHECK_CLOSE(track.dir.y, 0.8f, tol_percent);
  BOOST_CHECK_EQUAL(track.dir.x, track.enddir.x);
  BOOST_CHECK_EQUAL(track.dir.y, track.enddir.y);
  BOOST_CHECK_EQUAL(track.dir.z, track.enddir.z);
  BOOST_CHECK_EQUAL(track.time, 12.5);
  BOOST_CHECK_CLOSE(track.len_cm, 5.f, tol_percent);
  BOOST_CHECK_EQUAL(track.charge, -1); // mu- -> charge/3 == -1
  BOOST_CHECK_EQUAL(track.qual, 1.f);
  BOOST_REQUIRE_EQUAL(track.truth.size(), 1u);
}

BOOST_AUTO_TEST_CASE(track_charge_sign_follows_pdg) {
  ::caf::SRTrueParticle mu_plus;
  mu_plus.pdg              = -13; // mu+
  mu_plus.p.E              = 1.f;
  auto const mu_plus_track = track_from_true(mu_plus, make_id(), nullptr);
  BOOST_CHECK_EQUAL(mu_plus_track.charge, 1);

  ::caf::SRTrueParticle gamma;
  gamma.pdg              = 22; // neutral
  gamma.p.E              = 1.f;
  auto const gamma_track = track_from_true(gamma, make_id(), nullptr);
  BOOST_CHECK_EQUAL(gamma_track.charge, 0);
}

BOOST_AUTO_TEST_CASE(shower_evis_stays_in_gev_and_direction_is_normalized) {
  ::caf::SRTrueParticle true_part;
  true_part.pdg  = 22; // gamma
  true_part.p.px = 1.f;
  true_part.p.py = 0.f;
  true_part.p.pz = 0.f;
  true_part.p.E  = 0.8f; // GeV

  auto const shower = shower_from_true(true_part, make_id());

  BOOST_CHECK_CLOSE(shower.Evis, 800.f, tol_percent); // MeV, not 0.8f GeV
  BOOST_CHECK_CLOSE(shower.direction.x, 1.f, tol_percent);
  BOOST_CHECK_SMALL(shower.direction.y, 1e-6f);
  BOOST_REQUIRE_EQUAL(shower.truth.size(), 1u);
  BOOST_CHECK_EQUAL(shower.truthOverlap.at(0), 1.f);
}

BOOST_AUTO_TEST_SUITE_END()

FIX_TEST_EXIT
