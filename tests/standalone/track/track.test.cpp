
#include "ufw/utils.hpp"
#include <Math/GenVector/AxisAngle.h>
#define BOOST_TEST_MODULE hdf5
#include <boost/test/included/unit_test.hpp>

#include <common/track.h>

#include <random>

using track = sand::reco::track;

class Random3D {
  std::mt19937 gen; // Mersenne Twister engine

  // Spherical coordinate generation (uniform distribution)
  std::uniform_real_distribution<> theta_dist = std::uniform_real_distribution<>(0.0, M_PI);
  std::uniform_real_distribution<> phi_dist   = std::uniform_real_distribution<>(0.0, 2 * M_PI);
  std::normal_distribution<> angle_dist       = std::normal_distribution<>(0.0, 1.0);

 public:
  // Constructor
  explicit Random3D(unsigned seed = 0) : gen(seed) {}

  sand::dir_3d direction(double length) {
    double theta = theta_dist(gen);
    double phi   = phi_dist(gen);

    double x = length * std::sin(theta) * std::cos(phi);
    double y = length * std::sin(theta) * std::sin(phi);
    double z = length * std::cos(theta);

    return sand::dir_3d{x, y, z};
  }

  sand::dir_3d biased_direction(double length, sand::dir_3d bias) {
    sand::dir_3d base = direction(1.0);
    double k          = std::sqrt(bias.Mag2());
    if (std::abs(k) < 1.e-10) {
      return base * length;
    }
    bias         = bias / k;
    double angle = std::acos(base.Dot(bias));
    auto axis    = base.Cross(bias);
    angle *= 1.0 + angle_dist(gen) / k;
    // Create the rotation
    ROOT::Math::AxisAngle rot(axis, angle);
    return (rot * base) * length;
  }
};

BOOST_AUTO_TEST_CASE(empty_track) {
  track t;
  BOOST_CHECK_CLOSE(t.length(), 0.0, 1.e-9);
}

BOOST_AUTO_TEST_CASE(quota_splitting) {
  // a track is created with quota 1
  auto t1 = sand::reco::shared::create<track>();
  BOOST_CHECK_EQUAL(t1->quota(), 1.0);
  BOOST_CHECK(t1->is_unique());

  // a hit is split 0.25/0.75 across two tracks
  auto hit = sand::reco::shared::create<sand::reco::hit>(sand::pos_3d(0.0, 0.0, 5.0), sand::dir_3d(0.0, 0.0, 1.0),
                                                         sand::reco::timerange(0.0, 1.0), 2.0);
  auto [h_a, h_b] = sand::reco::shared::split(std::move(hit), 0.25);
  BOOST_CHECK_EQUAL(h_a->quota(), 0.25);
  BOOST_CHECK_EQUAL(h_b->quota(), 0.75);

  auto t2 = sand::reco::shared::create<track>();
  t1->push(std::move(h_a));
  t2->push(std::move(h_b));

  // the accumulate_on_hits callback now sees a const hit&
  const double width =
      t1->accumulate_on_hits<double>([](double a, const sand::reco::hit& h) { return a + h.width(); }, 0.0);
  BOOST_CHECK_CLOSE(width, 2.0, 1.e-9);

  // give t1 a segment, to check it survives a split
  t1->push(track::segment{sand::dir_3d(0.0, 0.0, 10.0), 0.0, sand::dir_3d(0.0, 0.0, 0.0), 1.0, {0.0, 0.0, 1.0}});

  // splitting a track deep-copies its hits, preserving their quotas
  auto [t3, t4] = sand::reco::shared::split(std::move(t1), 0.25);
  BOOST_CHECK_EQUAL(t3->quota(), 0.25);
  BOOST_CHECK_EQUAL(t4->quota(), 0.75);
  BOOST_CHECK_CLOSE(t3->length(), 10.0, 1.e-9);
  BOOST_CHECK_CLOSE(t4->length(), 10.0, 1.e-9);
  const double hit_quota =
      t3->accumulate_on_hits<double>([](double a, const sand::reco::hit& h) { return a + h.quota(); }, 0.0);
  BOOST_CHECK_EQUAL(hit_quota, 0.25);
  const double hit_width =
      t4->accumulate_on_hits<double>([](double a, const sand::reco::hit& h) { return a + h.width(); }, 0.0);
  BOOST_CHECK_CLOSE(hit_width, 2.0, 1.e-9);
}

BOOST_AUTO_TEST_CASE(insertion) {
  Random3D ranvec(12345);
  track t;
  auto coarse = ranvec.direction(10.0);
  UFW_DEBUG("Bias direction = {}", coarse);
  sand::dir_3d sum;
  for (int i = 0; i != 2000; ++i) {
    auto ds  = ranvec.biased_direction(0.5, coarse);
    auto q_r = ranvec.direction(1e-4);
    sum += ds;
    track::segment s{ds, 0.0, q_r, 0.0, {0.25, 0.0, 0.5}};
    t.push(s);
  }
  UFW_DEBUG("Total displacement = {}", sum);
  BOOST_CHECK_CLOSE(t.length(), 1000.0, 1.e-9);
  coarse *= 100.0;
  auto delta = t.end() - t.begin();
  // these may fail for pathologic random numbers, if it happens, just allow for more than 10% error
  BOOST_CHECK_CLOSE(delta.x(), coarse.x(), 10.);
  BOOST_CHECK_CLOSE(delta.y(), coarse.y(), 10.);
  BOOST_CHECK_CLOSE(delta.z(), coarse.z(), 10.);
}
