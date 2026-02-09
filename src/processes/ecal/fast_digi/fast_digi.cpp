#include <fast_digi.hpp>

namespace sand::ecal {

  void fast_digi::configure(const ufw::config& cfg) { process::configure(cfg); }

  fast_digi::fast_digi() : process({}, {}) {
    UFW_DEBUG("Creating a ecal fast digitization process at {}", fmt::ptr(this));
  }

  void fast_digi::run() {}

} // namespace sand::ecal