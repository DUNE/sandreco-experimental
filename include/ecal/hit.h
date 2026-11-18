#pragma once

#include <common/hit.h>

#include <string>

namespace sand::ecal {

  /// Stub for the ecal hit: carries no real quantities yet, only a test-only dummy
  /// payload (a string) so the round-trip tests have an ecal-specific member to check.
  /// Replace with the real ecal hit.
  class hit : public reco::hit {
   public:
    hit() = default;

    hit(const pos_3d& c, const dir_3d& d, const time& t, double w, const std::string& test_string)
      : reco::hit(c, d, t, w), m_test_string(test_string) {}

    const std::string& test_string() const { return m_test_string; }

   private:
    std::string m_test_string; // test-only
  };

} // namespace sand::ecal

SAND_SHARED_COLLECTION(sand::ecal, hit, ecal_hits)
