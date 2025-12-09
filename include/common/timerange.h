
#pragma once

#include <ufw/utils.hpp>

#include <cmath>

namespace sand::reco {
  /**
   * The time class represent a time estimate for a hit with a best guess and a min-max range.
   * Use this class to represent time before precise fitting has established an accurate value.
   * Example, in the tracker, given a signal recorded at time T, the corresponding hit time would be from
   * T-max_drift to T, with a best value somewhere in between.
   */
  class timerange {
   public:
    timerange()
      : m_earliest(std::numeric_limits<double>::quiet_NaN()),
        m_best(std::numeric_limits<double>::quiet_NaN()),
        m_latest(std::numeric_limits<double>::quiet_NaN()) {}

    explicit timerange(double t, double sigma = 1.0) : m_earliest(t - sigma), m_best(t), m_latest(t + sigma) {}

    timerange(double e, double b, double l) : m_earliest(e), m_best(b), m_latest(e) {
      if (!*this) {
        UFW_ERROR("Inconsistent time interval: ({}, {}, {}).", e, b, l);
      }
    }

    explicit operator bool() const { return !std::isnan(m_best) && m_earliest < m_best && m_best < m_latest; }

    double best() const { return m_best; }
    double earliest() const { return m_earliest; }
    double latest() const { return m_latest; }

    timerange& operator+= (double shift) {
      m_earliest += shift;
      m_best += shift;
      m_latest += shift;
      return *this;
    }

    timerange& operator-= (double shift) {
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

  inline timerange operator+ (const timerange& lhs, double shift) {
    timerange t(lhs);
    return t += shift;
  }

  inline timerange operator- (const timerange& lhs, double shift) {
    timerange t(lhs);
    return t -= shift;
  }

  inline bool operator< (const timerange& lhs, const timerange& rhs) { return lhs.best() < rhs.best(); }

  inline double operator- (const timerange& lhs, const timerange& rhs) { return lhs.best() - rhs.best(); }

  inline bool consistent(const timerange& lhs, const timerange& rhs) {
    return !(lhs.latest() < rhs.earliest() || rhs.latest() < lhs.earliest());
  }

  inline bool close(const timerange& lhs, const timerange& rhs) {
    return lhs.contains(rhs.best()) && rhs.contains(lhs.best());
  }

  inline double distance(const timerange& lhs, const timerange& rhs) {
    if (lhs < rhs) {
      return rhs.earliest() - lhs.latest();
    } else {
      return lhs.earliest() - rhs.latest();
    }
  }

  inline double overlap(const timerange& lhs, const timerange& rhs) { return -distance(lhs, rhs); }

} // namespace sand::reco
