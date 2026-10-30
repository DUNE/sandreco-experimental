#define BOOST_TEST_MODULE tracker_fast_reco_values
#include <boost/test/included/unit_test.hpp>

#include <processes/tracker/fast_reco/common_utils.hpp>

#include <test_helpers.hpp>

#include <duneanaobj/StandardRecord/SREnums.h>
#include <duneanaobj/StandardRecord/SRShower.h>
#include <duneanaobj/StandardRecord/SRTrack.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>
#include <duneanaobj/StandardRecord/SRVector3D.h>

#include <EDepSim/TG4Trajectory.h>

#include <TLorentzVector.h>
#include <TVector3.h>

#include <cmath>
#include <vector>

namespace {

  using sand::fast_reco::energy_deposit;
  using sand::fast_reco::shower_from;
  using sand::fast_reco::track_from;
  using sand::fast_reco::volume_hits_in;
  using sand::fast_reco::VolumeHits;

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

  /// A trajectory point at (x, y, z [mm], t [ns]) with momentum (px, py, pz [MeV]).
  EDEPTrajectoryPoint make_point(double x, double y, double z, double t, double px, double py, double pz) {
    TG4TrajectoryPoint g4_point;
    g4_point.Position = TLorentzVector{x, y, z, t};
    g4_point.Momentum = TVector3{px, py, pz};
    return EDEPTrajectoryPoint{g4_point};
  }

  /// What a particle with `hits` in the STT and trajectory `points` leaves in the tracker volumes.
  VolumeHits in_tracker(std::vector<EDEPHit> const& hits, EDEPTrajectoryPoints const& points = {}) {
    return volume_hits_in(EDEPHitsMap{{sand::subdetector_t::STT, hits}}, points,
                          {sand::subdetector_t::STT, sand::subdetector_t::DRIFT});
  }

  /// The hits of `hit_map` in the tracker volumes.
  std::vector<EDEPHit> tracker_hits(EDEPHitsMap const& hit_map) {
    return volume_hits_in(hit_map, {}, {sand::subdetector_t::STT, sand::subdetector_t::DRIFT}).hits;
  }

  double constexpr muon_mass_mev = 105.6583755;

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
  true_part.p.E       = 1.2f; // GeV: not used anymore, the tracker sees the particle at its entry
  true_part.time      = 12.5;
  true_part.start_pos = ::caf::SRVector3D{-100.f, -100.f, -100.f};
  true_part.end_pos   = ::caf::SRVector3D{100.f, 100.f, 100.f};

  auto const track = track_from(in_tracker(two_hits()), true_part, make_id());

  // from the hits: mm -> cm, MeV
  BOOST_CHECK_CLOSE(track.start.x, 1.f, tol_percent);
  BOOST_CHECK_CLOSE(track.start.y, 2.f, tol_percent);
  BOOST_CHECK_CLOSE(track.start.z, 3.f, tol_percent);
  BOOST_CHECK_CLOSE(track.end.z, 6.f, tol_percent);  // stop of the last hit
  BOOST_CHECK_EQUAL(track.time, 5.);                 // start of the first hit
  BOOST_CHECK_CLOSE(track.len_cm, 3.f, tol_percent); // 10 + 20 mm
  BOOST_CHECK_CLOSE(track.Evis, 0.5f, tol_percent);  // 0.2 + 0.3 MeV

  // no trajectory points in the tracker: directions from the first/last hit segment (both along +z), E unknown
  BOOST_CHECK_SMALL(track.dir.x, 1e-6f);
  BOOST_CHECK_CLOSE(track.dir.z, 1.f, tol_percent);
  BOOST_CHECK_CLOSE(track.enddir.z, 1.f, tol_percent);
  BOOST_CHECK(std::isnan(track.E));

  // from the truth
  BOOST_CHECK_EQUAL(track.charge, -1); // mu- -> charge/3 == -1
  BOOST_CHECK_EQUAL(track.qual, 1.f);
  BOOST_REQUIRE_EQUAL(track.truth.size(), 1u);
  BOOST_CHECK_EQUAL(track.truthOverlap.at(0), 1.f);
}

BOOST_AUTO_TEST_CASE(track_direction_and_energy_at_tracker_entry_and_exit) {
  ::caf::SRTrueParticle mu;
  mu.pdg  = 13;
  mu.p.E  = 1.2f; // GeV, at the vertex: the tracker sees less
  mu.p.px = 1.f;  // at the vertex, along x: the tracker sees it along y/z

  EDEPTrajectoryPoints points;
  points[sand::subdetector_t::GRAIN] = {make_point(0., 0., 0., 1., 1000., 0., 0.)};    // before the tracker: ignored
  points[sand::subdetector_t::STT]   = {make_point(10., 20., 30., 5., 0., 300., 400.), // entry, |p| = 500 MeV
                                        make_point(10., 20., 60., 7., 0., 0., 450.)};  // exit, along +z
  points[sand::subdetector_t::ECAL]  = {make_point(10., 20., 90., 9., 1000., 0., 0.)}; // after the tracker: ignored

  auto const track = track_from(in_tracker(two_hits(), points), mu, make_id());

  BOOST_CHECK_SMALL(track.dir.x, 1e-6f);
  BOOST_CHECK_CLOSE(track.dir.y, 0.6f, tol_percent); // (0, 300, 400) / 500
  BOOST_CHECK_CLOSE(track.dir.z, 0.8f, tol_percent);
  BOOST_CHECK_SMALL(track.enddir.y, 1e-6f);
  BOOST_CHECK_CLOSE(track.enddir.z, 1.f, tol_percent);
  BOOST_CHECK_CLOSE(track.E, std::hypot(500., muon_mass_mev), tol_percent); // MeV, at the tracker entry
}

BOOST_AUTO_TEST_CASE(track_stopping_in_the_tracker_takes_enddir_from_its_last_two_points) {
  ::caf::SRTrueParticle mu;
  mu.pdg = 13;

  EDEPTrajectoryPoints points;
  points[sand::subdetector_t::DRIFT] = {make_point(0., 0., 0., 1., 0., 0., 100.),
                                        make_point(0., 0., 10., 2., 0., 0., 50.),  // second-last
                                        make_point(0., 10., 10., 3., 0., 0., 0.)}; // last: stopped, moved along +y

  auto const track = track_from(in_tracker(two_hits(), points), mu, make_id());

  BOOST_CHECK_SMALL(track.enddir.x, 1e-6f);
  BOOST_CHECK_CLOSE(track.enddir.y, 1.f, tol_percent);
  BOOST_CHECK_SMALL(track.enddir.z, 1e-6f);
}

BOOST_AUTO_TEST_CASE(track_charge_sign_follows_pdg) {
  ::caf::SRTrueParticle mu_plus;
  mu_plus.pdg              = -13; // mu+
  auto const mu_plus_track = track_from(in_tracker(two_hits()), mu_plus, make_id());
  BOOST_CHECK_EQUAL(mu_plus_track.charge, 1);

  ::caf::SRTrueParticle gamma;
  gamma.pdg              = 22; // neutral
  auto const gamma_track = track_from(in_tracker(two_hits()), gamma, make_id());
  BOOST_CHECK_EQUAL(gamma_track.charge, 0);
}

BOOST_AUTO_TEST_CASE(shower_takes_start_time_and_evis_from_the_hits) {
  auto const shower = shower_from(in_tracker(two_hits()), make_id());

  // from the hits
  BOOST_CHECK_CLOSE(shower.start.x, 1.f, tol_percent);
  BOOST_CHECK_CLOSE(shower.start.z, 3.f, tol_percent);
  BOOST_CHECK_EQUAL(shower.time, 5.);
  BOOST_CHECK_CLOSE(shower.Evis, 0.5f, tol_percent); // deposited [MeV], not the particle's energy

  // no trajectory points in the tracker: direction from the first hit segment
  BOOST_CHECK_CLOSE(shower.direction.z, 1.f, tol_percent);
  BOOST_REQUIRE_EQUAL(shower.truth.size(), 1u);
  BOOST_CHECK_EQUAL(shower.truthOverlap.at(0), 1.f);
}

BOOST_AUTO_TEST_CASE(shower_direction_at_tracker_entry) {
  EDEPTrajectoryPoints points;
  points[sand::subdetector_t::STT] = {make_point(10., 20., 30., 5., 3., 4., 0.)};

  auto const shower = shower_from(in_tracker(two_hits(), points), make_id());

  BOOST_CHECK_CLOSE(shower.direction.x, 0.6f, tol_percent);
  BOOST_CHECK_CLOSE(shower.direction.y, 0.8f, tol_percent);
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
  BOOST_CHECK_EQUAL(energy_deposit(VolumeHits{hits}), 0.);
  BOOST_CHECK(tracker_hits(EDEPHitsMap{}).empty());
}

BOOST_AUTO_TEST_CASE(energy_deposit_is_the_sum_of_the_hits) {
  std::vector<EDEPHit> const hits{make_hit(1., 1.), make_hit(2., 2.), make_hit(3., 3.)}; // MeV

  BOOST_CHECK_CLOSE(energy_deposit(VolumeHits{hits}), 6., tol_percent);
}

BOOST_AUTO_TEST_SUITE_END()

FIX_TEST_EXIT
