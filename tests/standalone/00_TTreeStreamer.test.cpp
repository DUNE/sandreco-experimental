#define BOOST_TEST_MODULE TFileStreamer
#include <boost/test/included/unit_test.hpp>

#include <ufw/config.hpp>
#include <example1.h>
#include <common/root/TTreeStreamer.hpp>

#include <TFile.h>
#include <TTree.h>

using sand::common::root::TTreeStreamer;

BOOST_AUTO_TEST_CASE(streamer_write) {
  TTreeStreamer ts;
  ufw::config cfg;
  BOOST_TEST(ts.operation() == ufw::op_type::none);
  cfg["uri"] = "../../Testing/Temporary/f_00.root";
  cfg["tree"] = "mytree";
  ufw::type_id t("sand::example1");
  ts.configure(cfg, t.c_str(), ufw::op_type::wo);
  BOOST_TEST(ts.operation() == ufw::op_type::wo);
  sand::example1 ex;
  ex.uid = 42;
  ex.times.push_back(1);
  ex.times.push_back(2);
  ex.times.push_back(3);
  ts.attach(ex);
  ts.write(0);
  ex.uid = 43;
  ts.write(1);
  ex.uid = 44;
  ts.write(2);
}

BOOST_AUTO_TEST_CASE(streamer_read) {
  TTreeStreamer ts;
  ufw::config cfg;
  BOOST_TEST(ts.operation() == ufw::op_type::none);
  cfg["uri"] = "../../Testing/Temporary/f_00.root";
  cfg["tree"] = "mytree";
  ufw::type_id t("sand::example1");
  ts.configure(cfg, t.c_str(), ufw::op_type::ro);
  BOOST_TEST(ts.operation() == ufw::op_type::ro);
  sand::example1 ex;
  ts.attach(ex);
  ts.read(0);
  BOOST_TEST(ex.uid == 42);
  BOOST_TEST(ex.times[0] == 1);
  BOOST_TEST(ex.times[1] == 2);
  BOOST_TEST(ex.times[2] == 3);
  ts.read(1);
  BOOST_TEST(ex.uid == 43);
  ts.read(2);
  BOOST_TEST(ex.uid == 44);
}
