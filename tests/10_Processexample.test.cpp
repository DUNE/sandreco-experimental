#include <object.hpp>
#define BOOST_TEST_MODULE TTreeDataexample
#include <boost/test/included/unit_test.hpp>

#include <example.h>
#include <factory.hpp>
#include <process.hpp>

#include <TTreeData.hpp>
#include <TFileStreamer.hpp>

#include <TFile.h>
#include <TTree.h>

UFW_REGISTER_DATA(sand::example, sand::common::TTreeData<sand::example>)

class Process_example : public ufw::process {

public:
  void configure (const ufw::config& cfg) override;

  ufw::data_list products() const override;

  ufw::data_list requirements() const override;

  void run(const ufw::data_set&, ufw::data_set&) override;

private:
  double scale = 0.0;
};

UFW_REGISTER_PROCESS(Process_example)

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(Process_example)

void Process_example::configure (const ufw::config& cfg) {
  scale = cfg.value("scale", 1.0);
}

ufw::data_list Process_example::products() const {
  return ufw::data_list{{"output", "sand::example"}};
}

ufw::data_list Process_example::requirements() const {
  return ufw::data_list{{"input", "sand::example"}};
}

void Process_example::run(const ufw::data_set& input, ufw::data_set& output) {
  sand::example ex = ufw::data_cast<const sand::example>(*input.at("input"));
  ex.base *= scale;
  ufw::data_cast<sand::example>(*output.at("output")) = ex;
}

double calculateAverage(const char* fileName) {
    TFile file(fileName, "READ");
    TTree* tree = (TTree*)file.Get("mytree");
    sand::example* value = nullptr;
    tree->SetBranchAddress("myexample", &value);
    double sum = 0.0;
    auto count = tree->GetEntries();
    for (Long64_t i = 0; i < count; ++i) {
        tree->GetEntry(i);
        sum += value->base;
    }
    return (count > 0) ? (sum / count) : 0.0;
}

BOOST_AUTO_TEST_CASE(create) {
  ::setenv("ROOT_LIBRARY_PATH", "../src/data/example", 0);
  ufw::factory::add_search_path("../src/data/example");
  ufw::config tdecfg;
  tdecfg["name"] = "mytree";
  tdecfg["branch"] = "myexample";
  auto tderptr = std::shared_ptr<sand::common::TTreeData<sand::example>>(ufw::factory::create<sand::common::TTreeData<sand::example>>("sand::common::TTreeData<sand::example>", tdecfg).release());
  auto tdewptr = std::shared_ptr<sand::common::TTreeData<sand::example>>(ufw::factory::create<sand::common::TTreeData<sand::example>>("sand::common::TTreeData<sand::example>", tdecfg).release());

  ufw::config tfsrcfg;
  tfsrcfg["file"] = "../Testing/Temporary/f_05.root";
  tfsrcfg["mode"] = "READ";
  auto tfsrptr = ufw::factory::create<sand::common::TFileStreamer>("sand::common::TFileStreamer", tfsrcfg);

  ufw::config tfswcfg;
  tfswcfg["file"] = "../Testing/Temporary/f_10.root";
  tfswcfg["mode"] = "RECREATE";
  auto tfswptr = ufw::factory::create<sand::common::TFileStreamer>("sand::common::TFileStreamer", tfswcfg);

  ufw::config proccfg;
  proccfg["scale"] = 2.0;
  auto procptr = ufw::factory::create<Process_example>("Process_example", proccfg);

  //execution
  tfsrptr->read(*tderptr);
  tderptr->select(nullptr);
  tdewptr->select(nullptr); //does nothing but mimicks action::execute
  ufw::data_set in{{"input", tderptr}};
  ufw::data_set out{{"output", tdewptr}};
  BOOST_TEST(tderptr->entries() == 10000);
  for (std::size_t i = 0; i != tderptr->entries(); ++i) {
    tderptr->select(i);
    tdewptr->select(i);
    procptr->run(in, out);
  }
  tfswptr->write(*tdewptr);
  double f05 = calculateAverage("../Testing/Temporary/f_05.root") * double(proccfg["scale"]);
  double f10 = calculateAverage("../Testing/Temporary/f_10.root");
  BOOST_CHECK_CLOSE(f05, f10, 1e-6);
}
