#pragma once

#include <common/sand.h>
#include <common/shared.h>
#include <common/timerange.h>

namespace sand::reco {

  /**
   * The hit class represents a generic reconstructed hit in the detector.
   * Specialized classes for each detector must inherit from hit and are stored in
   * per-context shared stores (see SAND_SHARED_COLLECTION), addressed through typed keys.
   * The accessors are virtual so a hit can be used polymorphically through the base
   * interface, e.g. when a record holds keys into several stores.
   */
  class hit : public shared_object {
   public:
    using time = timerange;

    struct match {
      double axis_distance;
      double centre_distance;
      double time_distance;
    };

   public:
    hit() = default;

    hit(const pos_3d& c, const dir_3d& d, const time& t, double w)
      : m_centre(c), m_primary_axis(d), m_time(t), m_width(w) {}

    virtual dir_3d axis() const { return m_primary_axis; }

    virtual pos_3d centre() const { return m_centre; }

    virtual time t() const { return m_time; }

    virtual double width() const { return m_width; }

   private:
    pos_3d m_centre;
    dir_3d m_primary_axis;
    time m_time;
    double m_width;
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

} // namespace sand::reco

// The base hit type is streamable on its own: derived hit types stream their
// quantities through it, and the ROOT streamer picks this up when it builds its linkdef.
UFW_DECLARE_UNMANAGED_DATA(sand::reco::hit)
