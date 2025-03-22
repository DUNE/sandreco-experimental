#define BOOST_TEST_MODULE TFileStreamer
#include <boost/test/included/unit_test.hpp>

#include <config.hpp>
#include <TTreeStreamer.hpp>

#include <TNamed.h>

BOOST_AUTO_TEST_CASE(streamer_write) {/*
  sand::common::TObjectWrapper tow;
  auto named = new TNamed("myobjname", "My Object Title");
  tow.setObject(named, true);

  sand::common::TFileStreamer tfs;
  ufw::config cfg;
  cfg["file"] = "../Testing/Temporary/f_00.root";
  cfg["mode"] = "RECREATE";
  tfs.configure(cfg);

  tfs.write(tow);*/
}

BOOST_AUTO_TEST_CASE(streamer_read) {
  // sand::common::TObjectWrapper tow;
  // ufw::config towcfg;
  // towcfg["name"] = "myobjname";
  // tow.configure(towcfg);
  // 
  // sand::common::TFileStreamer tfs;
  // ufw::config tfscfg;
  // tfscfg["file"] = "../Testing/Temporary/f_00.root";
  // tfscfg["mode"] = "READ";
  // tfs.configure(tfscfg);
  // 
  // tfs.read(tow);
  // 
  // BOOST_TEST(tow.object() != nullptr);
  // 
  // BOOST_TEST(dynamic_cast<TNamed*>(tow.object()) != nullptr);
  // 
  // auto tn = static_cast<TNamed*>(tow.object());
  // 
  // BOOST_TEST(tn->GetName() == "myobjname");
  // 
  // BOOST_TEST(tn->GetTitle() == "My Object Title");
}
