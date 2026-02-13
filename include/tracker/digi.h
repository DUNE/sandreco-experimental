
#pragma once

#include <ufw/data.hpp>
#include <common/digi.h>
#include <common/sand.h>
#include <common/truth.h>

namespace sand::tracker {

  struct digi : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    /**
     * @brief A signal recorded by a tracker channel.
     */
    struct signal : reco::digi {

      /**
       * @brief Time-to-digital converter (TDC) value.
       * @unit
       */
      double tdc;

      /**
       * @brief Analog-to-digital converter (ADC) value.
       * @unit ADC counts (arbitrary units)
       */
      double adc;

        // Add equality operator
        // Equality operator
        bool operator==(const signal& other) const {
            // Compare base class (reco::digi) members
            if (channel() != other.channel()) return false;
            
            // Compare timerange objects - need to compare start/end or use a timerange equality operator
            // Assuming timerange has a way to compare (if not, you'll need to add it)
            const auto& t1 = t();
            const auto& t2 = other.t();
            
            // This assumes timerange has start()/end() methods or comparable members
            // You might need to adjust based on actual timerange interface
            if (t1.earliest() != t2.earliest() || t1.latest() != t2.latest()) return false;
            
            // Compare derived class members
            return tdc == other.tdc && 
                   adc == other.adc;
        }
        
        // Inequality operator
        bool operator!=(const signal& other) const {
            return !(*this == other);
        }
    };

    using signal_collection = std::vector<signal>;
    signal_collection signals;
  };
} // namespace sand::tracker

UFW_DECLARE_MANAGED_DATA(sand::tracker::digi)
