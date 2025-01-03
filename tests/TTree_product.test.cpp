#define BOOST_TEST_MODULE TTree_product
#include <boost/test/included/unit_test.hpp>

#include <iostream>
#include <random>

#include <example.h>
#include <TTree_product.hpp>

using TTree_test = sand::common::TTree_product<sand::example>;

BOOST_AUTO_TEST_CASE(tree_instance) {
  TTree_test ttt;
  ufw::config cfg;
  cfg["file"] = "../Testing/Temporary/f.root";
  cfg["branch"] = "ex";
  ttt.configure(cfg);
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
  TTree_test ttt;
  ufw::config cfg;
  cfg["file"] = "../Testing/Temporary/f.root";
  cfg["branch"] = "ex";
  ttt.configure(cfg);
  for (std::size_t i = 0; i != 10; ++i) {
    auto e = random_example();
    reference.push_back(e);
    ttt.select(i);
    ttt = e;
  }
  ttt.write();
}

bool operator == (const sand::example& lhs, const sand::example& rhs) {
  return lhs.uid == rhs.uid && lhs.base == rhs.base && lhs.weights == rhs.weights;
}

UFW_DECLARE_DATA_TYPE(sand::example) //this should be in a header, but it's ok since the test is a separate translation unit

BOOST_AUTO_TEST_CASE(tree_read) {
  TTree_test ttt;
  ufw::config cfg;
  cfg["file"] = "../Testing/Temporary/f.root";
  cfg["branch"] = "ex";
  ttt.configure(cfg);
  ttt.read();
  ttt.select(nullptr); //does nothing but mimicks action::execute
  BOOST_TEST(ttt.entries() == 10);
  for (std::size_t i = 0; i != ttt.entries(); ++i) {
    ttt.select(i);
    bool match = (reference[i] == ufw::data_cast<const sand::example>(ttt)); //const here is very important
    BOOST_TEST(match);
  }
}
