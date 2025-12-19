#pragma once

#include <common/sand.h>
#include <common/timerange.h>
#include <common/truth.h>
#include <cmath>

namespace sand::reco {

  /**
   * The digi class represents a generic digi in the detector.
   * Specialized classes for each detector must inherit from digi.
   */
  class digi : public sand::truth {
   public:
    using time = timerange;

   public:
    digi() : m_channel(), m_time(NAN) {}

    digi(sand::truth&& mc) : sand::truth(mc), m_channel(), m_time(NAN) {}

    digi(channel_id c, const time& t) : m_channel(c), m_time(t) {}

    digi(sand::truth&& mc, channel_id c, const time& t) : sand::truth(mc), m_channel(c), m_time(t) {}

    channel_id channel() const { return m_channel; }

    time t() const { return m_time; }

   private:
    channel_id m_channel;
    time m_time;
  };


} // namespace sand::reco
