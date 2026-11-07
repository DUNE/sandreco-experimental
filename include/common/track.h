#pragma once

#include <common/hit.h>
#include <common/sand.h>
#include <common/timerange.h>
#include <ecal/hit.h>
#include <grain/hit.h>
#include <tracker/hit.h>
#include <cmath>
#include <numeric>
#include <type_traits>

namespace sand::reco {

  /**
   * \class sand::reco::track
   *
   * \brief Most detailed working representation of a track
   *
   * The track class is used to represent a track in the highest level of detail.
   * The internal representation is based on two sets of objects, a set of typed keys
   * into the per-subdetector hit stores (one vector per store) and a list of
   * \p sand::reco::track::segments.
   * The track is composed of an ordered sequence of segments, each implicitly beginning at the end of the previous.
   * The first segment starts from begin()
   */
  class track : public managed_data_base {
   public:
    struct segment {
      dir_3d ds;       //< segment [mm]
      double sigma_xs; //< one sigma uncertainty radius around ds [mm]
      dir_3d q_r;      //< charge-curvature [e/mm]
      double dE;       //< visible energy [MeV]
      timerange dT;    //< time span of the segment [ns]
    };

    track() = default;

    vec_4d begin() const { return m_begin; }

    vec_4d end() const { return m_end; }

    /// @brief Read-only access to the keys into the grain hit store, in stored (push) order.
    const std::vector<grain::grain_hits_index>& grain_hits() const { return m_grain_hits; }

    /// @brief Read-only access to the keys into the tracker hit store, in stored (push) order.
    const std::vector<tracker::tracker_hits_index>& tracker_hits() const { return m_tracker_hits; }

    /// @brief Read-only access to the keys into the ecal hit store, in stored (push) order.
    const std::vector<ecal::ecal_hits_index>& ecal_hits() const { return m_ecal_hits; }

    /**
     * Iterate over the hits of the track as if they were a single container of
     * \p sand::reco::hit: each key is resolved against its store and the hit is
     * passed to \p f through the polymorphic base interface.
     * The order in which the stores are visited is unspecified; use a view to
     * obtain a defined ordering.
     */
    template <typename Fn>
    void for_each_hit(Fn&& f) const {
      for (const auto& i : m_grain_hits) f(*i);
      for (const auto& i : m_tracker_hits) f(*i);
      for (const auto& i : m_ecal_hits) f(*i);
    }

    template <typename Acc, typename Fn>
    Acc accumulate_on_hits(Fn&& f, const Acc& init) const {
      Acc acc = init;
      for_each_hit([&](const hit& h) { acc = f(acc, h); });
      return acc;
    }

    template <typename Fn, typename T = std::invoke_result_t<Fn, const segment&>>
    T accumulate_on_segments(Fn&& f, const T& init = T()) const {
      return std::accumulate(m_segments.begin(), m_segments.end(), init, std::forward<Fn>(f));
    }

    double length() {
      return accumulate_on_segments([](double a, segment b) { return a + std::sqrt(b.ds.Mag2()); }, 0.0);
    }

    void push(const grain::grain_hits_index& i) { m_grain_hits.push_back(i); }

    void push(const tracker::tracker_hits_index& i) { m_tracker_hits.push_back(i); }

    void push(const ecal::ecal_hits_index& i) { m_ecal_hits.push_back(i); }

    void push(const segment& s) {
      m_segments.push_back(s);
      m_end += sand::vec_4d(s.ds.x(), s.ds.y(), s.ds.z(), s.dT.span());
    }

   private:
    std::vector<grain::grain_hits_index> m_grain_hits;
    std::vector<tracker::tracker_hits_index> m_tracker_hits;
    std::vector<ecal::ecal_hits_index> m_ecal_hits;
    std::vector<segment> m_segments;
    vec_4d m_begin;
    vec_4d m_end;
  };

} // namespace sand::reco

UFW_DECLARE_MANAGED_DATA(sand::reco::track)
