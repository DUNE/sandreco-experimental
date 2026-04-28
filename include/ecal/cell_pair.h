#pragma once

#include <optional>

//#include <ufw/data.hpp>
#include <ecal/digit.h>

namespace sand::ecal {

  /// @brief Digi-level hypothesis of a signal in one ECal cell.
  ///
  /// A cell_pair stores up to two digits associated to the two geometrical
  /// readout faces of the same physical cell.
  struct cell_pair {
    using digit = digits_container::digit;

    /// Digits on the two cell readout faces
    std::optional<digit> begin;
    std::optional<digit> end;

    bool has_begin() const { return begin.has_value(); }
    bool has_end() const { return end.has_value(); }
    bool empty() const { return !has_begin() && !has_end(); }
    bool complete() const { return has_begin() && has_end(); }
    bool incomplete() const { return has_begin() != has_end(); }

    const digit& either() const { return begin ? *begin : *end; }
    digit& either() { return begin ? *begin : *end; }
  };

} // namespace sand::ecal

UFW_DECLARE_UNMANAGED_DATA(sand::ecal::cell_pair);
