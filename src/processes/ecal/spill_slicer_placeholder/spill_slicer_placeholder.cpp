#include <spill_slicer_placeholder.hpp>
#include <ecal/digi.h>
#include <ecal/slice.h>

namespace sand::ecal {

  void spill_slicer_placeholder::configure(const ufw::config& cfg) { process::configure(cfg); }

  spill_slicer_placeholder::spill_slicer_placeholder()
    : process({{"digi", "sand::ecal::digi"}}, {{"slices", "sand::ecal::slices"}}) {
    UFW_DEBUG("Creating ECAL spill slicer process at {}", fmt::ptr(this));
  }

  void spill_slicer_placeholder::run() {
    UFW_DEBUG("Running ECAL spill slicer process at {}", fmt::ptr(this));
    auto& digi    = get<sand::ecal::digi>("digi");
    auto& slices = set<sand::ecal::slices>("slices");
    slices.collection.emplace_back(digi.signals);
  }
} // namespace sand::ecal