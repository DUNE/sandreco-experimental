#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

#include "spill_slicer.hpp"

#include <ufw/factory.hpp>

namespace sand::ecal {

  /**
   * \class sand::ecal::spill_slicer
   * \brief Creates time windows from the current spill's flat ECAL digits.
   *
   * The preliminary time window boundary-finding policy produces one window containing every digit.
   * An empty digit collection produces no windows. All digit times must be finite;
   * invalid times raise an error.
   *
   * Windows use [begin, end) membership. The upper edge is the the smallest possible double 
   * greater than the latest digit time, so the latest digit is also included.
   * The representative time is the midpoint of the observed digit-time extent.
   * The original flat digit collection, its ordering, and its payload are preserved.
   *
   * No configured boundaries are required. compute_slice_times() is the extension
   * point for a future algorithm producing several ordered windows. Processes read
   * flat objects and the separate timeranges and apply reco::timeslicer locally.
   *
   * \subsection Requirements
   * | Name | Type | Description |
   * |------|------|-------------|
   * | `digi` | `sand::ecal::digits_container` | All digits in the current spill/context |
   *
   * \subsection Products
   * | Name | Type | Description |
   * |------|------|-------------|
   * | `timeranges` | `sand::reco::timeranges` | Collection of derived spill windows |
   */

  spill_slicer::spill_slicer()
    : process({{"digi", "sand::ecal::digits_container"}}, {{"timeranges", "sand::reco::timeranges"}}) {}

  void spill_slicer::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_print_output = cfg.value("print_output", false);
  }

  std::vector<reco::timerange>
  spill_slicer::compute_slice_times(const digits_container::digits_collection& digits) const {
    if (digits.empty()) {
      return {};
    }

    double earliest = std::numeric_limits<double>::infinity(); 
    double latest   = -std::numeric_limits<double>::infinity(); 
    for (std::size_t i = 0; i < digits.size(); ++i) {
      const double time = digits.at(i).tdc();
      if (!std::isfinite(time)) { 
        UFW_ERROR("ECAL spill_slicer: digit {} has a non-finite TDC time ({})", i, time);
      }
      earliest = std::min(earliest, time); 
      latest   = std::max(latest, time);
    }

    // The selection helper excludes the upper edge; expand it to include the last digit.
    // For the largest finite double, the next representable value is positive infinity.
    const double end = std::nextafter(latest, std::numeric_limits<double>::infinity());
    // Same-sign subtraction avoids overflow; opposite-sign addition preserves tiny times.
    const double best = std::signbit(earliest) == std::signbit(latest) ? earliest + (latest - earliest) * 0.5
                                                                       : (earliest + latest) * 0.5;
    return {reco::timerange(best, earliest, end)}; 
  }

  void spill_slicer::run() {
    const auto& digi = get<digits_container>("digi");
    auto& output     = set<reco::timeranges>("timeranges");
    output.clear(); 
    const auto windows = compute_slice_times(digi.digits);
    output.assign(windows.begin(), windows.end()); 

    UFW_DEBUG("ECAL spill_slicer: {} input digits, {} derived time windows with begin {} and end {} time", digi.digits.size(), output.size(), output.front().earliest(), output.back().latest());
    if (m_print_output) { 
      std::vector<double> times;
      for (const auto& digit : digi.digits) {
        times.push_back(digit.tdc());
      }
      std::sort(times.begin(), times.end());
      std::ostringstream input;
      input << std::setprecision(17);
      for (const auto time : times) {
        input << ' ' << time;
      }
      UFW_INFO("ECAL spill {}: {} digits, {} windows", ufw::context::current()->id(), times.size(), output.size());
      UFW_INFO("  sorted digit times [ns]:{}", input.str());
      for (std::size_t i = 0; i < output.size(); ++i) {
        const auto& window = output.at(i);
        const auto selected = std::count_if(times.begin(), times.end(), [&window](double time) {
          return window.earliest() <= time && time < window.latest();
        });
        UFW_INFO("  window {}: [{:.17g}, {:.17g}) ns, best={:.17g} ns, digits={}", i,
                 window.earliest(), window.latest(), window.best(), selected);
      }
    }
  }

} // namespace sand::ecal

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::ecal::spill_slicer)
