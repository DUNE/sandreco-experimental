#include <spill_slicer_placeholder.hpp>
#include <ecal/digit.h>
#include <ecal/digit_slice.h>

#include <ufw/factory.hpp>

namespace sand::ecal {

  void spill_slicer_placeholder::configure(const ufw::config& cfg) { process::configure(cfg); }

  spill_slicer_placeholder::spill_slicer_placeholder()
   : process({{"digi", "sand::ecal::digits_container"}},
          {{"digit_slices", "sand::ecal::digit_slices_container"}}) {
    UFW_DEBUG("Creating ECAL digit spill slicer process at {}", fmt::ptr(this));
  }

  void spill_slicer_placeholder::run() {
    UFW_DEBUG("Running ECAL digit spill slicer process at {}", fmt::ptr(this));
    auto& digi   = get<sand::ecal::digits_container>("digi");
    auto& digit_slices = set<sand::ecal::digit_slices_container>("digit_slices");
    digit_slices.collection.emplace_back(digi.digits);
    UFW_INFO(
    "ECAL digit spill slicer: input digits = {}, output slices = {}",
    digi.digits.size(),
    digit_slices.collection.size()
  );
  }
} // namespace sand::ecal
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::ecal::spill_slicer_placeholder)
