#pragma once

#include <common/hit.h>
#include <common/sand.h>
#include <common/timerange.h>
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
   * The internal representation is based on two sets of objects, a list of \p sand::reco::hits and a list of
   * \p sand::reco::track::segments.
   * The track is composed of an ordered sequence of segments, each implicitly beginning at the end of the previous.
   * The first segment starts from begin()
   */
  class track {
   public:
    struct segment {
      dir_3d ds;       //< segment [mm]
      double sigma_xs; //< one sigma uncertainty radius around ds [mm]
      double dE;       //< visible energy [MeV]
      timerange dT;    //< time span of the segment [ns]
    };

    track() = default;

    vec_4d begin() const { return m_begin; }

    vec_4d end() const { return m_end; }

    template <typename Fn, typename T = std::invoke_result_t<Fn, const hit&>>
    T accumulate_on_hits(Fn&& f, const T& init = T()) const {
      return std::accumulate(m_hits.begin(), m_hits.end(), init, std::forward<Fn>(f));
    }

    template <typename Fn, typename T = std::invoke_result_t<Fn, const segment&>>
    T accumulate_on_segments(Fn&& f, const T& init = T()) const {
      return std::accumulate(m_segments.begin(), m_segments.end(), init, std::forward<Fn>(f));
    }

    double length() {
      return accumulate_on_segments([](double a, segment b) { return a + std::sqrt(b.ds.Mag2()); }, 0.0);
    }

    void push(const hit& h) { m_hits.push_back(h); }

    void push(const segment& s) {
      m_segments.push_back(s);
      m_end += sand::vec_4d(s.ds.x(), s.ds.y(), s.ds.z(), s.dT.span());
    }

   private:
    std::vector<hit> m_hits;
    std::vector<segment> m_segments;
    vec_4d m_begin;
    vec_4d m_end;
  };

} // namespace sand::reco
