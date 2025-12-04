
#define BOOST_TEST_MODULE hdf5
#include <boost/test/included/unit_test.hpp>

#include <data/common/hdf5/hdf5.hpp>
#include <test_helpers.hpp>

BOOST_AUTO_TEST_CASE(hdf5) {
  ufw::config cfg = ufw::json::parse(R"({ "uri" : "../../../tests/data/test_sysmatrix_small.h5" })");
  sand::hdf5::ndarray array(cfg);
  auto ds = array.datasets();
  BOOST_TEST(ds.size() == 2);
  BOOST_TEST(ds[0] == "cam_1");
  BOOST_TEST(ds[1] == "cam_2");
  auto nd1 = array.range("cam_1");
  auto nd2 = array.range("cam_2");
  sand::hdf5::ndarray::ndrange comp{4, 5, 6, 8};
  BOOST_CHECK_EQUAL_COLLECTIONS(nd1.begin(), nd1.end(), comp.begin(), comp.end());
  BOOST_CHECK_EQUAL_COLLECTIONS(nd2.begin(), nd2.end(), comp.begin(), comp.end());
  BOOST_TEST(nd1.flat_size() == 4 * 5 * 6 * 8);
  BOOST_TEST(nd2.flat_size() == 4 * 5 * 6 * 8);
  std::unique_ptr<float[]> ptr1(new float[4 * 5 * 6 * 8]);
  std::vector<float> vec(4 * 5 * 6 * 8, 0.f);
  array.read("cam_1", ptr1.get());
  array.read("cam_2", vec);
  // large tolerance because I took these from a printout with only a few digits
  BOOST_CHECK_CLOSE(ptr1[0], -0.25092, 2.e-4);
  BOOST_CHECK_CLOSE(ptr1[7], 0.732352, 2.e-4);
  BOOST_CHECK_CLOSE(ptr1[4 * 5 * 6 * 8 - 1], 0.476067, 2.e-4);
  BOOST_CHECK_CLOSE(vec[0], -0.834403, 2.e-4);
  BOOST_CHECK_CLOSE(vec[7], -0.405757, 2.e-4);
  BOOST_CHECK_CLOSE(vec.back(), -0.174756, 2.e-4);
  sand::test_exit();
}
