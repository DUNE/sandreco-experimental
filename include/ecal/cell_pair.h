#pragma once

#include <common/data.h>
#include <ecal/digit.h>

namespace sand::ecal {

  /// @brief Digi-level pairing hypothesis of a signal in one ECAL cell.
  ///
  /// A cell_pair stores up to two digits associated to the two geometrical
  /// readout faces of the same physical ECAL cell IN THE SAME TIME WINDOW.
  struct cell_pair {
    using digit = digits_container::digit;

    /// Digits on the two begin/end readout faces; ignore a digit when its presence flag is false.
    digit begin;
    digit end;
    bool begin_present = false;
    bool end_present   = false;

    /// True if the same physical cell has more than one physically valid
    /// complete begin/end pair within the same digit time window.
    ///
    /// This allows downstream reconstruction to identify cells with ambiguous
    /// complete-pair candidates.
    bool cell_has_competing_complete_pairs = false;

    /// Number of physically valid complete begin/end pairs found in the same
    /// physical cell within the same digit time window.
    ///
    /// For unambiguous complete pairs this is 1.
    /// For ambiguous complete pairs this is > 1.
    /// For incomplete pairs this is the number of valid complete pairs found
    /// in the same cell, 0 if the digit was fully unmatched.
    std::uint16_t n_complete_candidates_in_cell = 0;

    bool has_begin() const { return begin_present; }
    bool has_end() const { return end_present; }

    bool empty() const { return !has_begin() && !has_end(); }

    bool complete() const { return has_begin() && has_end(); }

    bool incomplete() const { return has_begin() != has_end(); }

    bool unambiguous_complete() const { return complete() && !cell_has_competing_complete_pairs; }

    bool ambiguous_complete() const { return complete() && cell_has_competing_complete_pairs; }

    const digit& either() const { return begin_present ? begin : end; }

    digit& either() { return begin_present ? begin : end; }
  };

} // namespace sand::ecal

SAND_DATA_COLLECTION(sand::ecal, cell_pair, cell_pairs)
