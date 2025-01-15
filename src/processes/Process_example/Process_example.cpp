#include <example.h>
#include <factory.hpp>
#include <process.hpp>
#include <product.hpp>

#include <TTree_product.hpp>

UFW_DECLARE_DATA_TYPE(sand::example)

class Process_example : public ufw::process {

private:
  ufw::product_list products() const override;

  ufw::product_list requirements() const override;

  void run(const ufw::product_set&, ufw::product_set&) override;

};

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(Process_example)


ufw::product_list Process_example::products() const {
  return ufw::product_list{{"output", "sand::example-TTree_example"}};
}

ufw::product_list Process_example::requirements() const {
  return ufw::product_list{{"input", "sand::example-TTree_example"}};
}

void Process_example::run(const ufw::product_set& input, ufw::product_set& output) {
  check_configured(); 
  auto req = std::dynamic_pointer_cast<const sand::common::TTree_product<sand::example>>(input.at("input"));
  std::cerr << "requirement = " << req.get();
  std::cerr << "\nm_branchname = " << req->m_branchname;
  std::cerr << "\nm_branch = " << req->m_branch;
  std::cerr << "\nm_data = " << req->m_data << std::endl;
  sand::example ex = ufw::data_cast<sand::example>(*input.at("input"));
  ex.base *= 2.0;
  auto prod = std::dynamic_pointer_cast<sand::common::TTree_product<sand::example>>(output.at("output"));
  std::cerr << "product = " << prod.get();
  std::cerr << "\nm_branchname = " << prod->m_branchname;
  std::cerr << "\nm_branch = " << prod->m_branch;
  std::cerr << "\nm_data = " << prod->m_data << std::endl;
  //ufw::data_cast<sand::example>(*output.at("output")) = ex;
}
