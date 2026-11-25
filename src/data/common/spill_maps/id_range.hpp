#ifndef SAND_ID_RANGE_HPP
#define SAND_ID_RANGE_HPP

#include <algorithm>
#include <limits>
#include <optional>

namespace sand {

  /// Smallest and largest of a set of distinct ids, and how many they are: the accumulator used to build an IdRange.
  struct IdBounds {
    int min{std::numeric_limits<int>::max()};
    int max{std::numeric_limits<int>::min()};
    int count{};
  };

  constexpr IdBounds id_bounds_of(int id) noexcept { return {id, id, 1}; }

  /// Bounds of the union of two disjoint sets of ids.
  constexpr IdBounds merge_id_bounds(IdBounds const& a, IdBounds const& b) noexcept {
    return {std::min(a.min, b.min), std::max(a.max, b.max), a.count + b.count};
  }

  /// Half-open range [first, first + count) of edep-sim track ids.
  struct IdRange {
    int first{};
    int count{};
    constexpr bool contains(int id) const noexcept { return id >= first && id < first + count; }
  };

  /// @return the range spanned by `ids`, or nothing if they are not contiguous.
  constexpr std::optional<IdRange> contiguous_id_range(IdBounds const& ids) noexcept {
    if (ids.count == 0) {
      return IdRange{};
    }
    if (ids.max - ids.min + 1 != ids.count) {
      return std::nullopt;
    }
    return IdRange{ids.min, ids.count};
  }

} // namespace sand

#endif
