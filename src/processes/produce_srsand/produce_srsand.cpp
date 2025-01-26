#include <data.hpp>
#include <factory.hpp>

#include <duneanaobj/StandardRecord/SRSAND.h>

#include <produce_srsand.hpp>

UFW_REGISTER_PROCESS(sand::process::produce_srsand)

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::process::produce_srsand)

namespace sand {

  namespace process {

    void produce_srsand::configure (const ufw::config& cfg) {
    }

    ufw::data_list produce_srsand::products() const {
      return ufw::data_list{{"output", "caf::SRSANDInt"}};
    }

    ufw::data_list produce_srsand::requirements() const {
      return ufw::data_list{};
    }

    void produce_srsand::run(const ufw::data_set& input, ufw::data_set& output) {
      ufw::data_cast<caf::SRSANDInt>(*output.at("output")).ntracks = 1;
    }

  }

}
