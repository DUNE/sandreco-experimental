#pragma once

#include <vector>

#include <common/timerange.h>
#include <ecal/digit.h>
#include <ufw/process.hpp>

namespace sand::ecal {

  /// Derives spill windows from flat ECAL digits without changing the digit collection.
  class spill_slicer : public ufw::process {
   public:
    spill_slicer();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    bool m_print_output = false;

    /// Boundary-finding policy: currently one window covering every input digit.
    /// Future algorithms can return several ordered windows through the same interface.
    std::vector<reco::timerange> compute_slice_times(const digits_container::digits_collection& digits) const;
  };

} // namespace sand::ecal

UFW_REGISTER_PROCESS(sand::ecal::spill_slicer)
