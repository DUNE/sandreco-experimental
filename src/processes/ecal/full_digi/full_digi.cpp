#include <full_digi.hpp>
#include <geoinfo/ecal_info.hpp>
#include <ecal/digit.h>
#include <ecal/photo_electron.h>

#include <ufw/factory.hpp>

namespace sand::ecal {
  void full_digi::configure(const ufw::config& cfg) {
    process::configure(cfg);
    // example configuration parameter
    m_config_param = cfg.at("config_param");
  }

  full_digi::full_digi() : process({{"pes", "sand::ecal::pes_container"}}, {{"digi", "sand::ecal::digits_container"}}) {
    UFW_DEBUG("Creating a ecal full digitization process at {}", fmt::ptr(this));
  }

  void full_digi::run() {
    UFW_DEBUG("Running a ecal full digitization process at {}", fmt::ptr(this));
    // Get ECAL geometry information
    const auto& gecal = get<geoinfo>().ecal();
    // Get input photo-electron collection
    auto& pes = get<sand::ecal::pes_container>("pes");
    // Get output digitized signal collection
    auto& digi = set<sand::ecal::digits_container>("digi");

    // Process photo-electrons for each PMT channel
    for (auto [pmt, pe_collection] : pes.collection) {
      // Sort photo-electrons by arrival time for temporal processing
      std::sort(pe_collection.begin(), pe_collection.end(),
                [](const auto& a, const auto& b) { return a.arrival_time < b.arrival_time; });
      // TODO: implement full digitization
    }
  }
} // namespace sand::ecal
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::ecal::full_digi)
