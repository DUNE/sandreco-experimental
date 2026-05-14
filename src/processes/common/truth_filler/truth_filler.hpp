#ifndef SAND_TRUTH_FILLER_HPP
#define SAND_TRUTH_FILLER_HPP

#include <caf/caf_wrapper.hpp>
#include <edep_reader/edep_reader.hpp>
#include <genie_reader/genie_reader.hpp>

#include <ufw/process.hpp>

namespace sand {

  struct truth_filler : public ufw::process {
    truth_filler();
    void configure(ufw::config const& cfg) override;
    void run() override;
  };

} // namespace sand

UFW_REGISTER_PROCESS(sand::truth_filler);

#endif
