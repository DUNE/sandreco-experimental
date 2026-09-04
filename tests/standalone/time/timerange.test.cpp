#define BOOST_TEST_MODULE hdf5
#include <boost/test/included/unit_test.hpp>

#include <common/timeslice.h>

using namespace sand::reco;
BOOST_AUTO_TEST_CASE(timeslice) {
  std::vector<timerange> tr;
  tr.emplace_back(1.0, 5.0, 10.0);
  tr.emplace_back(4.0, 6.0, 9.0);
  timeslice ts;
}
