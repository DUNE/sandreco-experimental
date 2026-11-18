#pragma once

#include <common/hit.h>

namespace sand::tracker {

  /// Stub for the tracker hit: carries no real quantities yet, only a test-only dummy
  /// payload (a double) so the round-trip tests have a tracker-specific member to check.
  /// Replace with the real tracker hit.
  class hit : public reco::hit {
   public:
    hit() = default;

    hit(const pos_3d& c, const dir_3d& d, const time& t, double w, double test_double)
      : reco::hit(c, d, t, w), m_test_double(test_double) {}

    double test_double() const { return m_test_double; }

   private:
    double m_test_double; // test-only
  };

} // namespace sand::tracker

SAND_SHARED_COLLECTION(sand::tracker, hit, tracker_hits)
