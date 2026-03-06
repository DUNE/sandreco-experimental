#pragma once

#include <ufw/data.hpp>
#include <ecal/digit.h>

namespace sand::ecal {

  /** @struct slices_container
   * @brief Container for storing collections of ECAL slices.
   *
   * This struct inherits from a base class providing managed data functionality
   * and contains a collection of slices, where each slice is a collection of digits.
   */
  struct slices_container : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    /** @brief Type alias for a single slice, which is a collection of digits. */
    using slice = digits_container::digits_collection;

    /** @brief Type alias for a collection of slices. */
    using slice_collection = std::vector<slice>;

    /** @brief The collection of slices stored in this container. */
    slice_collection collection;
  };

} // namespace sand::ecal

UFW_DECLARE_MANAGED_DATA(sand::ecal::slices_container);
