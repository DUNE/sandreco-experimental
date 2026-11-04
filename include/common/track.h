#pragma once

#include <common/data.h>
#include <common/hit.h>
#include <common/sand.h>
#include <common/timerange.h>

#include <memory>
#include <numeric>
#include <utility>

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
   *
   * A track is a shared object: it is created via reco::shared::create and may only be duplicated
   * via reco::shared::split, which deep-copies its hits while preserving their quotas.
   */
  class track : public shared {
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

    template <typename T, typename Fn>
    T accumulate_on_hits(Fn&& f, const T& init = T()) const {
      return std::accumulate(m_hits.begin(), m_hits.end(), init,
                             [&f](const T& a, const std::unique_ptr<hit>& h) { return f(a, *h); });
    }

    template <typename T, typename Fn>
    T accumulate_on_segments(Fn&& f, const T& init = T()) const {
      return std::accumulate(m_segments.begin(), m_segments.end(), init, std::forward<Fn>(f));
    }

    double length() {
      return accumulate_on_segments([](double a, segment b) { return a + std::sqrt(b.ds.Mag2()); }, 0.0);
    }

    void push(std::unique_ptr<hit>&& h) { m_hits.push_back(std::move(h)); }

    void push(const segment& s) {
      m_segments.push_back(s);
      m_end += sand::vec_4d(s.ds.x(), s.ds.y(), s.ds.z(), s.dT.span());
    }

   protected:
    /// Deep copy of a track: hits are re-allocated with their quotas preserved, used by shared::split
    track(const track& other) : shared(other), m_segments(other.m_segments), m_begin(other.m_begin), m_end(other.m_end) {
      m_hits.reserve(other.m_hits.size());
      for (const auto& h : other.m_hits) {
        m_hits.push_back(clone_ptr(h));
      }
    }

    shared* copy_self() const override { return new track(*this); }

    std::vector<std::unique_ptr<hit>> m_hits;
    std::vector<segment> m_segments;
    vec_4d m_begin;
    vec_4d m_end;
  };

} // namespace sand::reco

SAND_DATA_SHARED_COLLECTION(sand::reco, track, tracks)
