#include <spill_slicer_placeholder.hpp>
#include <ecal/digit.h>
#include <ecal/slice.h>

namespace sand::ecal {

  void spill_slicer_placeholder::configure(const ufw::config& cfg) { process::configure(cfg); }

  spill_slicer_placeholder::spill_slicer_placeholder()
    : process({{"digi", "sand::ecal::digits_container"}}, {{"slices", "sand::ecal::slices"}}) {
    UFW_DEBUG("Creating ECAL spill slicer process at {}", fmt::ptr(this));
  }

  void spill_slicer_placeholder::run() {
    UFW_DEBUG("Running ECAL spill slicer process at {}", fmt::ptr(this));
    auto& digi   = get<sand::ecal::digits_container>("digi");
    auto& slices = set<sand::ecal::slices_container>("slices");
    slices.collection.emplace_back(digi.digits);
  }
} // namespace sand::ecal