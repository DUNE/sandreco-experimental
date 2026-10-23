#pragma once
#include <vector>
#include <utility>
#include <common/sand.h>
#include <common/digi.h>

namespace sand::reco {

  class waveform : public digi<sand::truth_index> {
   public:
    using digi_base_type = digi<sand::truth_index>;

    template <typename Func, typename... Args>
    void for_each(Func&& f, Args&&... args) const {
      for (size_t i = 0u; i != m_samples.size(); ++i) {
        std::forward<Func>(f)(m_samples[i], std::forward<Args...>(args)...);
      }
    }

    waveform() : digi_base_type(), m_time_stop(NAN) {}

    waveform(channel_id c, time t_start, time t_stop, source src, size_t n_samples)
      : digi_base_type(c, t_start, src), m_time_stop(t_stop), m_samples(n_samples) {}

    double& at(size_t idx) { return m_samples.at(idx); }
    double at(size_t idx) const { return m_samples.at(idx); }

    size_t size() const { return m_samples.size(); }

    auto begin() { return m_samples.begin(); }
    auto end() { return m_samples.end(); }
    auto begin() const { return m_samples.begin(); }
    auto end() const { return m_samples.end(); }

    double sample_period() const {
      if (m_samples.empty()) {
        return NAN;
      }
      return (m_time_stop - t()) / m_samples.size();
    }

   private:
    time m_time_stop;
    std::vector<double> m_samples;
  };
} // namespace sand::reco

SAND_DATA_COLLECTION(sand::reco, waveform, waveforms)
UFW_DECLARE_UNMANAGED_DATA(sand::reco::digi<sand::truth_index>)