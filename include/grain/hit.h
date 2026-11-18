#pragma once

#include <common/hit.h>

namespace sand::grain {

  /// Stub for the grain hit: carries no real quantities yet, only a test-only dummy
  /// payload (a float) so the round-trip tests have a grain-specific member to check.
  /// Replace with the real grain hit.
  class hit : public reco::hit {
   public:
    hit() = default;

    hit(const pos_3d& c, const dir_3d& d, const time& t, double w, float test_float)
      : reco::hit(c, d, t, w), m_test_float(test_float) {}

    float test_float() const { return m_test_float; }

   private:
    float m_test_float; // test-only
  };

} // namespace sand::grain

SAND_SHARED_COLLECTION(sand::grain, hit, grain_hits)
