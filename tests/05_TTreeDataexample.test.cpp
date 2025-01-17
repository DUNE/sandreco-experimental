#define BOOST_TEST_MODULE TTreeDataexample
#include <boost/test/included/unit_test.hpp>

#include <random>

#include <example.h>
#include <factory.hpp>
#include <TTreeData.hpp>
#include <TFileStreamer.hpp>

using TTreeDataexample = sand::common::TTreeData<sand::example>;

BOOST_AUTO_TEST_CASE(tree_instance) {
  TTreeDataexample tde;
  ufw::config cfg;
  cfg["name"] = "mytree";
  cfg["branch"] = "myexample";
  tde.configure(cfg);
}


sand::example random_example() {
  static std::mt19937 generator;
  static std::uniform_int_distribution<std::size_t> flat(10, 100);
  static std::normal_distribution<double> gaus(5.0,2.0);
  sand::example e;
  e.uid = flat(generator);
  e.base = gaus(generator);
  auto n = flat(generator);
  while (--n > 0) {
    e.weights.push_back(gaus(generator));
  }
  return e;
}

static std::vector<sand::example> reference;

BOOST_AUTO_TEST_CASE(tree_write) {

  sand::common::TFileStreamer tfs;
  ufw::config tfscfg;
  tfscfg["file"] = "../Testing/Temporary/f_05.root";
  tfscfg["mode"] = "RECREATE";
  tfs.configure(tfscfg);

  TTreeDataexample tde;
  ufw::config tdecfg;
  tdecfg["name"] = "mytree";
  tdecfg["branch"] = "myexample";
  tde.configure(tdecfg);
  for (std::size_t i = 0; i != 10000; ++i) {
    auto e = random_example();
    reference.push_back(e);
    tde.select(i);
    tde = e;
  }

  tfs.write(tde);
}


bool operator == (const sand::example& lhs, const sand::example& rhs) {
  return lhs.uid == rhs.uid && lhs.base == rhs.base && lhs.weights == rhs.weights;
}

UFW_REGISTER_DATA(sand::example, sand::common::TTreeData<sand::example>)  //this should be in a header, but it's ok since the test is a separate translation unit

BOOST_AUTO_TEST_CASE(tree_read) {

  ufw::factory::add_search_path("/home/ntosi/Development/sandreco-exp-build/tests/");
  sand::common::TFileStreamer tfs;
  ufw::config tfscfg;
  tfscfg["file"] = "../Testing/Temporary/f_05.root";
  tfscfg["mode"] = "READ";
  tfs.configure(tfscfg);

  ufw::config tdecfg;
  tdecfg["name"] = "mytree";
  tdecfg["branch"] = "myexample";
  auto tdeptr = ufw::factory::create<sand::common::TTreeData<sand::example> >("sand::common::TTreeData<sand::example>", tdecfg);
  auto& tde = *tdeptr;
  tfs.read(tde);

  tde.select(nullptr); //does nothing but mimicks action::execute

  BOOST_TEST(tde.entries() == 10000);
  for (std::size_t i = 0; i != tde.entries(); ++i) {
    tde.select(i);
    bool match = (reference[i] == ufw::data_cast<const sand::example>(tde)); //const here is very important
    BOOST_TEST(match);
  }
}
