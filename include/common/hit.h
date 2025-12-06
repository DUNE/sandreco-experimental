#pragma once

#include <common/sand.h>
#include <common/truth.h>

namespace sand::reco {

  /**
   * The hit class represents a generic reconstructed hit in the detector.
   * Specialized classes for each detector must inherit from hit.
   */
  class hit : public true_hits {
   public:
    /**
     * The time class represent a time estimate for a hit with a best guess and a min-max range.
     * Use this class to represent time before precise fitting has established an accurate value.
     * Example, in the tracker, given a signal recorded at time T, the corresponding hit time would be from
     * T-max_drift to T, with a best value somewhere in between.
     */
    class time {
     public:
      time()
        : m_earliest(std::numeric_limits<double>::quiet_NaN()),
          m_best(std::numeric_limits<double>::quiet_NaN()),
          m_latest(std::numeric_limits<double>::quiet_NaN()) {}

      explicit time(double t, double sigma = 1.0) : m_earliest(t - sigma), m_best(t), m_latest(t + sigma) {}

      time(double e, double b, double l) : m_earliest(e), m_best(b), m_latest(e) {
        if (!*this) {
          UFW_ERROR("Inconsistent time interval: ({}, {}, {}).", e, b, l);
        }
      }

      explicit operator bool() const { return !std::isnan(m_best) && m_earliest < m_best && m_best < m_latest; }

      double best() const { return m_best; }
      double earliest() const { return m_earliest; }
      double latest() const { return m_latest; }

      time& operator += (double shift) {
        m_earliest += shift;
        m_best += shift;
        m_latest += shift;
        return *this;
      }

      time& operator -= (double shift) {
        m_earliest -= shift;
        m_best -= shift;
        m_latest -= shift;
        return *this;
      }

      bool contains(double t) const { return m_earliest <= t && t <= m_latest; }

     private:
      double m_earliest;
      double m_best;
      double m_latest;
    };

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

  inline hit::time operator + (const hit::time& lhs, double shift) {
    hit::time t(lhs);
    return t += shift;
  }

  inline hit::time operator - (const hit::time& lhs, double shift) {
    hit::time t(lhs);
    return t -= shift;
  }

  inline bool operator < (const hit::time& lhs, const hit::time& rhs) { return lhs.best() < rhs.best(); }

  inline double operator - (const hit::time& lhs, const hit::time& rhs) { return lhs.best() - rhs.best(); }

  inline bool consistent(const hit::time& lhs, const hit::time& rhs) {
    return !(lhs.latest() < rhs.earliest() || rhs.latest() < lhs.earliest());
  }

  inline bool close(const hit::time& lhs, const hit::time& rhs) {
    return lhs.contains(rhs.best()) && rhs.contains(lhs.best());
  }

  inline double distance(const hit::time& lhs, const hit::time& rhs) {
    if (lhs < rhs) {
      return rhs.earliest() - lhs.latest();
    } else {
      return lhs.earliest() - rhs.latest();
    }
  }

  inline double overlap(const hit::time& lhs, const hit::time& rhs) { return -distance(lhs, rhs); }

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

  inline bool operator < (const hit::match& lhs, const hit::match& rhs) {
    return lhs.axis_distance < rhs.axis_distance && lhs.centre_distance < rhs.centre_distance
        && lhs.time_distance < rhs.time_distance;
  }

} // namespace sand::reco
