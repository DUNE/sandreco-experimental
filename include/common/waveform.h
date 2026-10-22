#pragma once

#include <common/sand.h>

namespace sand::reco {

  class waveform : public reco::digi {
   public:
    template <typename Func, typename... Args>
    void for_each(Func&& f, Args&&... args) const {
      for (size_t i = 0u; i != m_samples.size(); ++i) {
        std::forward<Func>(f)(m_samples[i], std::forward<Args...>(args)...);
      }
    }

    waveform() : reco::digi(), m_time_stop(NAN) {}

    waveform(channel_id c, time t_start, time t_stop, source src, size_t n_samples)
      : reco::digi(c, t_start, src), m_time_stop(t_stop), m_samples(n_samples) {}

    double& at(size_t idx) { return m_samples.at(idx); }
    double at(size_t idx) const { return m_samples.at(idx); }

    double sample_period() const { return (m_time_start - m_time_stop) / m_size.size(); }

   private:
    time m_time_stop;
    std::vector<double> m_samples;
  }
} // namespace sand

SAND_DATA_COLLECTION(sand::reco, waveform, waveforms)
