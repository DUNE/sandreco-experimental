#ifndef SAND_COMMON_TRUTH_FILLER_HPP
#define SAND_COMMON_TRUTH_FILLER_HPP

#include <edep_reader/edep_reader.hpp>
#include <genie_reader/genie_reader.hpp>

#include <ufw/process.hpp>

namespace sand::common {

  struct truth_filler : public ufw::process {
    truth_filler();
    void run() override;
  };

} // namespace sand::common

UFW_REGISTER_PROCESS(sand::common::truth_filler);

#endif
