#ifndef SAND_COMMON_TRUTH_FILLER_TYPES_HPP
#define SAND_COMMON_TRUTH_FILLER_TYPES_HPP

#include <vector>

class EDEPTrajectory;

namespace sand::common {

  struct InteractionRange {
    std::size_t first_primary_index;
    std::size_t primary_count;
  };

  using Primaries = std::vector<EDEPTrajectory>;

  struct Kinematics {
    float Q2{};
    float q0{};
    float modq{};
    float W{};
    float bjorkenX{};
    float inelasticity{};
  };

} // namespace sand::common

#endif
