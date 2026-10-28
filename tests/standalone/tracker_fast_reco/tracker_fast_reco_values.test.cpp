#define BOOST_TEST_MODULE tracker_fast_reco_values
#include <boost/test/included/unit_test.hpp>

#include <processes/tracker/fast_reco/fast_reco.hpp>

#include <test_helpers.hpp>

#include <duneanaobj/StandardRecord/SREnums.h>
#include <duneanaobj/StandardRecord/SRShower.h>
#include <duneanaobj/StandardRecord/SRTrack.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>
#include <duneanaobj/StandardRecord/SRVector3D.h>

#include <vector>

namespace {

  using sand::tracker::shower_from_hits;
  using sand::tracker::total_energy_deposit;
  using sand::tracker::track_from_hits;
  using sand::tracker::tracker_hits;

  ::caf::TrueParticleID make_id(int part = 0) { return {0, ::caf::TrueParticleID::kPrimary, part}; }

  /// A hit starting at time `t` [ns] and depositing `edep` [MeV]; the hit index `id` tells hits apart.
  EDEPHit make_hit(double t, double edep, int id = 0) {
    return EDEPHit{sand::vec_4d{0., 0., 0., t}, sand::vec_4d{0., 0., 1., t + 1.}, edep, 0., 1., 1, 1, id};
  }

  /// A hit going from `start` to `stop` (x, y, z [mm], t [ns]), `length` [mm] long, depositing `edep` [MeV].
  EDEPHit make_segment(sand::vec_4d const& start, sand::vec_4d const& stop, double length, double edep) {
    return EDEPHit{start, stop, edep, 0., length, 1, 1, 0};
  }

  /// Two consecutive tracker hits along z: (10, 20, 30) mm at 5 ns -> (10, 20, 60) mm at 7 ns.
  std::vector<EDEPHit> two_hits() {
    using sand::vec_4d;
    return {make_segment(vec_4d{10., 20., 30., 5.}, vec_4d{10., 20., 40., 6.}, 10., 0.2),
            make_segment(vec_4d{10., 20., 40., 6.}, vec_4d{10., 20., 60., 7.}, 20., 0.3)};
  }

  std::vector<double> start_times(std::vector<EDEPHit> const& hits) {
    std::vector<double> times;
    for (auto const& hit : hits) {
      times.push_back(hit.GetStart().T());
    }
    return times;
  }

  double constexpr tol_percent = 1e-4;

} // namespace

BOOST_AUTO_TEST_SUITE(objects_from_hits)

BOOST_AUTO_TEST_CASE(track_takes_position_time_length_and_evis_from_the_hits) {
  ::caf::SRTrueParticle true_part;
  true_part.pdg       = 13; // mu-
  true_part.p.px      = 0.3f;
  true_part.p.py      = 0.4f;
  true_part.p.pz      = 0.f;
  true_part.p.E       = 1.2f; // GeV
  true_part.time      = 12.5; // not used anymore: the time comes from the first hit
  true_part.start_pos = ::caf::SRVector3D{-100.f, -100.f, -100.f}; // not used anymore either
  true_part.end_pos   = ::caf::SRVector3D{100.f, 100.f, 100.f};

  auto const track = track_from_hits(two_hits(), EDEPTrajectoryPoints{}, true_part, make_id());

  // from the hits: mm -> cm, MeV
  BOOST_CHECK_CLOSE(track.start.x, 1.f, tol_percent);
  BOOST_CHECK_CLOSE(track.start.y, 2.f, tol_percent);
  BOOST_CHECK_CLOSE(track.start.z, 3.f, tol_percent);
  BOOST_CHECK_CLOSE(track.end.z, 6.f, tol_percent);  // stop of the last hit
  BOOST_CHECK_EQUAL(track.time, 5.);                 // start of the first hit
  BOOST_CHECK_CLOSE(track.len_cm, 3.f, tol_percent); // 10 + 20 mm
  BOOST_CHECK_CLOSE(track.Evis, 0.5f, tol_percent);  // 0.2 + 0.3 MeV

  // still from the truth
  BOOST_CHECK_CLOSE(track.E, 1200.f, tol_percent);   // MeV, not 1.2f GeV
  BOOST_CHECK_CLOSE(track.dir.x, 0.6f, tol_percent); // normalize_to_direction(0.3, 0.4, 0)
  BOOST_CHECK_CLOSE(track.dir.y, 0.8f, tol_percent);
  BOOST_CHECK_EQUAL(track.dir.x, track.enddir.x); // no trajectory points -> enddir == dir
  BOOST_CHECK_EQUAL(track.dir.y, track.enddir.y);
  BOOST_CHECK_EQUAL(track.dir.z, track.enddir.z);
  BOOST_CHECK_EQUAL(track.charge, -1); // mu- -> charge/3 == -1
  BOOST_CHECK_EQUAL(track.qual, 1.f);
  BOOST_REQUIRE_EQUAL(track.truth.size(), 1u);
  BOOST_CHECK_EQUAL(track.truthOverlap.at(0), 1.f);
}

BOOST_AUTO_TEST_CASE(track_charge_sign_follows_pdg) {
  ::caf::SRTrueParticle mu_plus;
  mu_plus.pdg              = -13; // mu+
  mu_plus.p.E              = 1.f;
  auto const mu_plus_track = track_from_hits(two_hits(), EDEPTrajectoryPoints{}, mu_plus, make_id());
  BOOST_CHECK_EQUAL(mu_plus_track.charge, 1);

  ::caf::SRTrueParticle gamma;
  gamma.pdg              = 22; // neutral
  gamma.p.E              = 1.f;
  auto const gamma_track = track_from_hits(two_hits(), EDEPTrajectoryPoints{}, gamma, make_id());
  BOOST_CHECK_EQUAL(gamma_track.charge, 0);
}

BOOST_AUTO_TEST_CASE(shower_takes_start_time_and_evis_from_the_hits) {
  ::caf::SRTrueParticle true_part;
  true_part.pdg  = 11; // e-
  true_part.p.px = 1.f;
  true_part.p.py = 0.f;
  true_part.p.pz = 0.f;
  true_part.p.E  = 0.8f; // GeV: only a fraction of it is deposited in the tracker

  auto const shower = shower_from_hits(two_hits(), true_part, make_id());

  // from the hits
  BOOST_CHECK_CLOSE(shower.start.x, 1.f, tol_percent);
  BOOST_CHECK_CLOSE(shower.start.z, 3.f, tol_percent);
  BOOST_CHECK_EQUAL(shower.time, 5.);
  BOOST_CHECK_CLOSE(shower.Evis, 0.5f, tol_percent); // deposited [MeV], not the true 800 MeV

  // still from the truth
  BOOST_CHECK_CLOSE(shower.direction.x, 1.f, tol_percent);
  BOOST_CHECK_SMALL(shower.direction.y, 1e-6f);
  BOOST_REQUIRE_EQUAL(shower.truth.size(), 1u);
  BOOST_CHECK_EQUAL(shower.truthOverlap.at(0), 1.f);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(tracker_hits_selection)

BOOST_AUTO_TEST_CASE(hits_come_out_sorted_by_start_time) {
  EDEPHitsMap hit_map;
  hit_map[sand::subdetector_t::STT] = {make_hit(3., 0.1), make_hit(1., 0.1), make_hit(2., 0.1)};

  auto const hits = tracker_hits(hit_map);

  std::vector<double> const expected{1., 2., 3.};
  auto const times = start_times(hits);
  BOOST_CHECK_EQUAL_COLLECTIONS(times.begin(), times.end(), expected.begin(), expected.end());
}

BOOST_AUTO_TEST_CASE(only_stt_and_drift_hits_are_kept_and_merged_in_time) {
  EDEPHitsMap hit_map;
  hit_map[sand::subdetector_t::STT]   = {make_hit(4., 0.1), make_hit(1., 0.1)};
  hit_map[sand::subdetector_t::DRIFT] = {make_hit(3., 0.1), make_hit(2., 0.1)};
  hit_map[sand::subdetector_t::ECAL]  = {make_hit(0.5, 10.)};
  hit_map[sand::subdetector_t::GRAIN] = {make_hit(0.1, 10.)};

  auto const hits = tracker_hits(hit_map);

  std::vector<double> const expected{1., 2., 3., 4.}; // no ECAL (0.5) or GRAIN (0.1) hit
  auto const times = start_times(hits);
  BOOST_CHECK_EQUAL_COLLECTIONS(times.begin(), times.end(), expected.begin(), expected.end());
}

BOOST_AUTO_TEST_CASE(no_tracker_hits_gives_nothing) {
  EDEPHitsMap hit_map;
  hit_map[sand::subdetector_t::ECAL] = {make_hit(1., 10.)};

  auto const hits = tracker_hits(hit_map);

  BOOST_CHECK(hits.empty());
  BOOST_CHECK_EQUAL(total_energy_deposit(hits), 0.);
  BOOST_CHECK(tracker_hits(EDEPHitsMap{}).empty());
}

BOOST_AUTO_TEST_CASE(total_energy_deposit_is_the_sum_of_the_hits) {
  std::vector<EDEPHit> const hits{make_hit(1., 1.), make_hit(2., 2.), make_hit(3., 3.)}; // MeV

  BOOST_CHECK_CLOSE(total_energy_deposit(hits), 6., tol_percent);
}

BOOST_AUTO_TEST_SUITE_END()

FIX_TEST_EXIT
