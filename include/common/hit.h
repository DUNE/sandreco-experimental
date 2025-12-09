#pragma once

#include <common/sand.h>
#include <common/timerange.h>
#include <common/truth.h>

namespace sand::reco {

  /**
   * The hit class represents a generic reconstructed hit in the detector.
   * Specialized classes for each detector must inherit from hit.
   */
  class hit {
   public:
    using time = timerange;

    struct match {
      double axis_distance;
      double centre_distance;
      double time_distance;
    };

   public:
    hit(const pos_3d& c, const dir_3d& d, const time& t) : m_centre(c), m_primary_axis(d), m_time(t) {}

    dir_3d axis() const { return m_primary_axis; }

    pos_3d centre() const { return m_centre; }

    time t() const { return m_time; }

    double width() const { return m_width; }

   private:
    pos_3d m_centre;
    dir_3d m_primary_axis;
    double m_width;
    time m_time;
  };

  inline hit::match match(const hit& lhs, const hit& rhs) {
    hit::match r;
    auto dist    = lhs.centre() - rhs.centre();
    auto cross   = lhs.axis().Cross(rhs.axis());
    double denom = cross.Mag2();
    if (denom < 1.e-20) {
      auto cross2     = dist.Cross(lhs.axis());
      r.axis_distance = cross2.R() / lhs.axis().R();
    } else {
      r.axis_distance = std::abs(dist.Dot(cross)) / std::sqrt(denom);
    }
    r.centre_distance = (lhs.centre() - rhs.centre()).R();
    r.time_distance   = distance(lhs.t(), rhs.t());
    return r;
  }

  inline bool operator< (const hit::match& lhs, const hit::match& rhs) {
    return lhs.axis_distance < rhs.axis_distance && lhs.centre_distance < rhs.centre_distance
        && lhs.time_distance < rhs.time_distance;
  }

  template <typename HitType>
  class hits_base {
    static_assert(std::is_base_of_v<hit, HitType>, "HitType must inherit from sand::reco::hit");
  };

} // namespace sand::reco
