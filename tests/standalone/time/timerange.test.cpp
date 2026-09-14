#include <boost/test/tools/old/interface.hpp>
#define BOOST_TEST_MODULE hdf5
#include <boost/test/included/unit_test.hpp>

#include <common/digi.h>
#include <common/timeslice.h>

#include <random>

using namespace sand::reco;

BOOST_AUTO_TEST_CASE(time_range) {
  timerange tr1(5.0, 1.0, 10.0);
  timerange tr2(6.0, 4.0, 9.0);
  BOOST_TEST(tr1 < tr2);
}

timerange gen_tr() {
  static std::mt19937 gen(42);

  std::uniform_real_distribution<double> dist_centre(-500.0, 15000.0);
  std::uniform_real_distribution<double> dist_low(0.1, 500.0);
  std::uniform_real_distribution<double> dist_high(0.1, 500.0);
  double centre = dist_centre(gen);
  double low    = centre - dist_low(gen);
  double high   = centre + dist_high(gen);

  return timerange(centre); //, low, high);
}

BOOST_AUTO_TEST_CASE(test_timerange_random_samples) {
  const int num_samples = 1000;

  BOOST_CHECK_NO_THROW(gen_tr());
  std::vector<timerange> vec;

  for (int i = 0; i < num_samples; ++i) {
    vec.push_back(gen_tr());
  }
  std::sort(vec.begin(), vec.end());
  int overlap     = 0;
  int non_overlap = 0;
  for (auto it = vec.begin(); it != std::prev(vec.end()); ++it) {
    if (distance(*it, *std::next(it)) > 0.0) {
      non_overlap++;
    } else {
      overlap++;
    }
  }
  BOOST_TEST(overlap + non_overlap == num_samples - 1);
}

BOOST_AUTO_TEST_CASE(time_slice) {
  std::vector<timerange> vec;
  const int num_slices = 10;
  for (int i = 0; i < num_slices; ++i) {
    vec.push_back(gen_tr());
  }
  BOOST_TEST(!std::is_sorted(vec.begin(), vec.end()));
  BOOST_REQUIRE_THROW(timeslices{vec}, ufw::exception);
  std::sort(vec.begin(), vec.end());
  BOOST_REQUIRE_NO_THROW(timeslices{vec});
  std::mt19937 gen(1337);
  std::uniform_real_distribution<double> dist_centre(-500.0, 15000.0);

  const int num_samples = 1000;
  using digi            = sand::reco::digi<sand::truth_index>;
  std::vector<digi> digis;

  for (int i = 0; i < num_samples; ++i) {
    digis.emplace_back(sand::channel_id{.raw = -1ul}, dist_centre(gen), digi::source::unknown);
  }
  timeslices ts{vec};
  auto slices = ts.slice(digis.begin(), digis.end());
  //check we did not create more slices than necessary
  BOOST_REQUIRE_LE(std::distance(slices.begin(), slices.end()), vec.size());
  //check all elements of digis that are in a slice are inside that slice
  for (auto slice : slices) {
    for (const auto& d : slice) {
      BOOST_TEST(slice.contains(d.t()));
    }
  }
  //check that no elements of digis that are not in a slice are not supposed to be
  for (auto it = digis.begin(); it != digis.end(); ++it) {
    for (auto slice : vec) {
      BOOST_TEST(!slice.contains(it->t()));
    }
  }
}
