#pragma once

#include <ecal/digit.h>

namespace sand::ecal {

  /** @struct digit_slices_container
   * @brief Container for storing collections of ECAL digit slices.
   *
   * This struct inherits from a base class providing managed data functionality
   * and contains a collection of slices, where each slice is a collection of digits.
   */
  struct digit_slices_container : managed_data_base {
    /** @brief Type alias for a single slice, which is a collection of digits. */
    using digit_slice = digits_container::digits_collection;

    /** @brief Type alias for a collection of slices. */
    using digit_slice_collection = std::vector<digit_slice>;

    /** @brief The collection of slices stored in this container. */
    digit_slice_collection collection;
  };

} // namespace sand::ecal

UFW_DECLARE_MANAGED_DATA(sand::ecal::digit_slices_container);
