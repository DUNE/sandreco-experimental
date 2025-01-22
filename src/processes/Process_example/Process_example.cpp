#include <example.h>
#include <data.hpp>
#include <factory.hpp>
#include <process.hpp>

#include <TTreeData.hpp>

UFW_REGISTER_DATA(sand::example, sand::common::TTreeData<sand::example>)

class Process_example : public ufw::process {

public:
  void configure (const ufw::config& cfg) override;

  ufw::data_list products() const override;

  ufw::data_list requirements() const override;

  void run(const ufw::data_set&, ufw::data_set&) override;

private:
  double m_scale = 0.0;
};

UFW_REGISTER_PROCESS(Process_example)

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(Process_example)

void Process_example::configure (const ufw::config& cfg) {
  m_scale = cfg.value("scale", 1.0);
}

ufw::data_list Process_example::products() const {
  return ufw::data_list{{"output", "sand::example"}};
}

ufw::data_list Process_example::requirements() const {
  return ufw::data_list{{"input", "sand::example"}};
}

void Process_example::run(const ufw::data_set& input, ufw::data_set& output) {
  sand::example ex = ufw::data_cast<const sand::example>(*input.at("input"));
  ex.base *= m_scale;
  ufw::data_cast<sand::example>(*output.at("output")) = ex;
}
