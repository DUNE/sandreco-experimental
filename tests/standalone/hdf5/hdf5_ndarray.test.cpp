
#define BOOST_TEST_MODULE hdf5
#include <boost/test/included/unit_test.hpp>

#include <data/common/hdf5/hdf5.hpp>
#include <test_helpers.hpp>

static std::vector<float> vec(4 * 5 * 6 * 8, 0.f);
std::unique_ptr<float[]> ptr1(new float[4 * 5 * 6 * 8]);

BOOST_AUTO_TEST_CASE(hdf5_read) {
  UFW_INFO("Reading from hdf5!");
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
  array.read("cam_1", ptr1.get());
  array.read("cam_2", vec);
  auto attr1 = array.attribute("cam_1", "attr_1");
  BOOST_TEST(attr1 == "val_1");
  auto attr2 = array.attribute("cam_2", "attr_2");
  BOOST_TEST(attr2 == "val_2");
  // large tolerance because I took these from a printout with only a few digits
  BOOST_CHECK_CLOSE(ptr1[0], -0.25092, 2.e-4);
  BOOST_CHECK_CLOSE(ptr1[7], 0.732352, 2.e-4);
  BOOST_CHECK_CLOSE(ptr1[4 * 5 * 6 * 8 - 1], 0.476067, 2.e-4);
  BOOST_CHECK_CLOSE(vec[0], -0.834403, 2.e-4);
  BOOST_CHECK_CLOSE(vec[7], -0.405757, 2.e-4);
  BOOST_CHECK_CLOSE(vec.back(), -0.174756, 2.e-4);
}

BOOST_AUTO_TEST_CASE(hdf5_write) {
  UFW_INFO("Writing to hdf5!");
  ufw::config cfg = ufw::json::parse(R"({ "uri" : "test_write_small.h5", "io" : "overwrite" })");
  sand::hdf5::ndarray array(cfg);
  sand::hdf5::ndarray::ndrange range({4, 5, 6, 8});
  range.set_type(H5::PredType::NATIVE_FLOAT);
  array.write("cam_1", range, ptr1.get());
  array.set_attribute("cam_1", "attr_1", "val_1");
  array.write("cam_2", range, vec);
  array.set_attribute("cam_2", "attr_2", "val_2");
  UFW_INFO("Wrote to file!");
}

FIX_TEST_EXIT
