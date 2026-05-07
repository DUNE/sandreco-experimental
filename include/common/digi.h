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
  template <typename T = sand::truth_index>
  class digi : public sand::truth<T> {

  public:
    using time = timerange;
    using truth_type = typename sand::truth<T>;
    using true_hit_type = typename truth_type::true_hit_type;

    enum class source {
      unknown,
      sim,
      det
    };

   public:
    digi() : m_channel(), m_time(NAN), m_source(source::unknown) {}

    digi(truth_type&& mc) : truth_type(mc), m_channel(), m_time(NAN), m_source(source::sim) {}

    digi(channel_id c, const time& t, source src) : m_channel(c), m_time(t), m_source(src) {}

    digi(truth_type&& mc, channel_id c, const time& t) : truth_type(mc), m_channel(c), m_time(t), m_source(source::sim) {}

    channel_id channel() const { return m_channel; }

    source data_source() const { return m_source; }

    time t() const { return m_time; }

    bool operator==(const digi& other) const {
        return truth_type::operator==(other)
            && m_channel == other.m_channel
            && m_time    == other.m_time
            && m_source  == other.m_source;
    }

    bool operator!=(const digi& other) const {
        return !(*this == other);
    }

   private:
    channel_id m_channel;
    time m_time;
    source m_source;
  };


} // namespace sand::reco
