#define BOOST_TEST_MODULE time
#include <boost/test/included/unit_test.hpp>

#include <common/digi.h>
#include <common/timeslice.h>

#include <random>

using namespace sand::reco;

timerange gen_tr() {
  static std::mt19937 gen(42);

  std::uniform_real_distribution<double> dist_centre(-500.0, 15000.0);
  std::uniform_real_distribution<double> dist_low(0.1, 2500.0);
  std::uniform_real_distribution<double> dist_high(0.1, 2500.0);
  double centre = dist_centre(gen);
  double low    = centre - dist_low(gen);
  double high   = centre + dist_high(gen);

  return timerange(centre, low, high);
}

BOOST_AUTO_TEST_CASE(time_range_random_samples) {
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

  //check we have some coverage
  double total_range = 0.0;
  for (auto range : vec) {
    total_range += range.span();
  }
  BOOST_REQUIRE_GT(total_range, 1000.0);
  std::mt19937 gen(1337);
  std::uniform_real_distribution<double> dist_centre(-500.0, 15000.0);

  const int num_samples = 100;
  using digi            = sand::reco::digi<sand::truth_index>;
  std::vector<digi> digis;

  for (int i = 0; i < num_samples; ++i) {
    digis.emplace_back(sand::channel_id{.raw = -1ul}, dist_centre(gen), digi::source::unknown);
  }
  std::sort(digis.begin(), digis.end(), [](auto lhs, auto rhs) { return lhs.t() < rhs.t(); });
  timeslices ts{vec};
  auto slices = ts.slice(digis.begin(), digis.end());
  //check that we created some non empty slice
  BOOST_REQUIRE_GT(slices.size(), 0);
  int in_slices = 0;
  for (auto slice : slices) {
    in_slices += std::distance(slice.begin(), slice.end());
  }
  BOOST_REQUIRE_GT(in_slices, 0);
  //check we did not create more slices than necessary
  BOOST_REQUIRE_LE(std::distance(slices.begin(), slices.end()), vec.size());
  //check all elements of digis that are in a slice are inside that slice
  for (auto slice : slices) {
    BOOST_REQUIRE_LT(slice.earliest(), slice.latest());
    for (const auto& d : slice) {
      BOOST_REQUIRE_GE(d.t(), slice.earliest());
      BOOST_REQUIRE_LE(d.t(), slice.latest());
    }
  }
  //check that no elements of digis that are not in a slice are supposed to be
  for (auto it = digis.begin(); it != digis.end(); ++it) {
    bool in_slice = false;
    for (auto slice : vec) {
      in_slice = in_slice || slice.contains(it->t());
    }
    if (in_slice) {
      continue;
    }
    for (auto slice : slices) {
      auto its = slice.begin();
      while (its != slice.end()) {
        bool b = it != its++;
        BOOST_TEST(b);
      }
    }
  }
  // Repeat for disjoint
  auto slices_d = ts.slice_disjoint(digis.begin(), digis.end());
  //check that we created some non empty slice
  BOOST_REQUIRE_GT(slices_d.size(), 0);
  in_slices = 0;
  for (auto slice : slices_d) {
    in_slices += std::distance(slice.begin(), slice.end());
  }
  BOOST_REQUIRE_GT(in_slices, 0);
  //check we did not create more slices than necessary
  BOOST_REQUIRE_LE(std::distance(slices_d.begin(), slices_d.end()), vec.size());
  //check all elements of digis that are in a slice are inside that slice
  for (auto slice : slices_d) {
    BOOST_REQUIRE_LT(slice.earliest(), slice.latest());
    for (const auto& d : slice) {
      BOOST_REQUIRE_GE(d.t(), slice.earliest());
      BOOST_REQUIRE_LE(d.t(), slice.latest());
    }
  }
  //check that no elements of digis that are not in a slice are supposed to be
  for (auto it = digis.begin(); it != digis.end(); ++it) {
    bool in_slice = false;
    for (auto slice : vec) {
      in_slice = in_slice || slice.contains(it->t());
    }
    if (in_slice) {
      continue;
    }
    for (auto slice : slices_d) {
      auto its = slice.begin();
      while (its != slice.end()) {
        bool b = it != its++;
        BOOST_TEST(b);
      }
    }
  }
}
