#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <edep_reader/edep_reader.hpp>
#include <geoinfo/drift_info.hpp>
#include <geoinfo/geoinfo.hpp>
#include <geoinfo/stt_info.hpp>
#include <geoinfo/tracker_info.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>
#include <tracker/digi.h>

#include <clustering.hpp>

namespace sand::tracker {

  void clustering::configure(const ufw::config& cfg) {
    process::configure(cfg);
  }

  clustering::clustering() : process({{"digi", "sand::tracker::digi"}}, {{"digi", "sand::tracker::digi"}}) {
    UFW_DEBUG("Creating a clustering process at {}", fmt::ptr(this));
  }

  void clustering::run() {
    UFW_DEBUG("Running clustering process at {}", fmt::ptr(this));
    const auto& tree = get<sand::edep_reader>();
    const auto& gi   = get<geoinfo>();
    const auto& digi = get<sand::tracker::digi>("digi");
    const auto& tgm        = ufw::context::current()->instance<root_tgeomanager>();
    auto & digi_out = set<sand::tracker::digi>("digi");


    for (const auto& signal : digi.signals) {
      tracker::digi::signal clustered_signal = signal; //TODO cluster signals
      auto & hits = clustered_signal.true_hits();
      UFW_DEBUG("Clustering signal with TDC {} and ADC {}.", static_cast<int>(signal.channel().channel), signal.tdc, signal.adc);
      const auto & chsignal = clustered_signal.channel();
      UFW_DEBUG("  Channel info: subdetector {}, link {}, channel {}.", 
                static_cast<int>(chsignal.subdetector), static_cast<int>(chsignal.link), static_cast<int>(chsignal.channel));
      const auto w = gi.tracker().wire_at(chsignal);
      UFW_DEBUG("Corresponding to wire with info: head {}, tail {}", w.head, w.tail);
      digi_out.signals.push_back(clustered_signal);
    }

    UFW_DEBUG(" STT subdetector implementation");

  }

 

} // namespace sand::tracker