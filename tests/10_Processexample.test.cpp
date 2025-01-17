#define BOOST_TEST_MODULE TTreeDataexample
#include <boost/test/included/unit_test.hpp>

#include <example.h>
#include <factory.hpp>
#include <process.hpp>

#include <TTreeData.hpp>
#include <TFileStreamer.hpp>

UFW_REGISTER_DATA(sand::example, sand::common::TTreeData<sand::example>)

BOOST_AUTO_TEST_CASE(create) {

  ufw::factory::add_search_path("/home/ntosi/Development/sandreco-exp-build/tests/");
  ufw::config tdecfg;
  tdecfg["name"] = "mytree";
  tdecfg["branch"] = "myexample";
  auto tdeptr = ufw::factory::create<sand::common::TTreeData<sand::example>>("sand::common::TTreeData<sand::example>", tdecfg);

}
