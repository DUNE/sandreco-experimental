#pragma once

#include <ecal/digit.h>
#include <optional>

namespace sand::ecal {

  /// @brief Digi-level pairing hypothesis of a signal in one ECAL cell.
  ///
  /// A cell_pair stores up to two digits associated to the two geometrical
  /// readout faces of the same physical ECAL cell.
  struct cell_pair {
    using digit = digits_container::digit;

    /// Digits on the two begin/end readout faces
    std::optional<digit> begin;
    std::optional<digit> end;

    /// True if the same physical cell has more than one physically valid
    /// complete begin/end pair.
    ///
    /// This allows downstream reconstruction to identify cells with ambiguous
    /// complete-pair candidates while preserving the same cell-level flag on
    /// incomplete recovery hypotheses.
    bool cell_has_competing_complete_pairs = false;

    /// Number of physically valid complete begin/end pairs found in the same
    /// physical cell.
    ///
    /// For unambiguous complete pairs this is 1.
    /// For ambiguous complete pairs this is > 1.
    /// For incomplete pairs this is the number of valid complete pairs found
    /// in the same cell, 0 if the digit was fully unmatched.
    std::uint16_t n_complete_candidates_in_cell = 0;

    bool has_begin() const { return begin.has_value(); }
    bool has_end() const { return end.has_value(); }

    bool empty() const { return !has_begin() && !has_end(); }

    bool complete() const { return has_begin() && has_end(); }

    bool incomplete() const { return has_begin() != has_end(); }

    bool unambiguous_complete() const { return complete() && !cell_has_competing_complete_pairs; }

    bool ambiguous_complete() const { return complete() && cell_has_competing_complete_pairs; }

    const digit& either() const { return begin ? *begin : *end; }

    digit& either() { return begin ? *begin : *end; }
  };

} // namespace sand::ecal

UFW_DECLARE_UNMANAGED_DATA(sand::ecal::cell_pair);