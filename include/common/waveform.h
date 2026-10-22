#pragma once

#include <common/sand.h>

namespace sand {

  class waveform {
   public:
    template <typename Func, typename... Args>
    void for_each(Func&& f, Args&&... args) const {
      for (size_t i = 0u; i != m_samples.size(); ++i) {
        std::forward<Func>(f)(m_samples[i], std::forward<Args...>(args)...);
      }
    }

    time t() const { return m_time_start; }

    double& at(size_t idx) { return m_samples.at(idx); }
    double at(size_t idx) const { return m_samples.at(idx); }

    double sample_period() const { return (m_time_start - m_time_stop) / m_size.size(); }

   private:
    channel_id m_channel;
    time m_time_start;
    time m_time_stop;
    source m_source;
    std::vector<double> m_samples;
  }
} // namespace sand