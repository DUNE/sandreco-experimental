//
// Created by Paolo Forni on 11/14/25.
//

#ifndef SANDRECO_CAF_WRAPPER_HPP
#define SANDRECO_CAF_WRAPPER_HPP

#include <common/data.h>

#include <duneanaobj/StandardRecord/StandardRecord.h>

namespace sand::caf {

  struct caf_wrapper
    : public ::caf::StandardRecord
    , public sand::managed_data_base {};

} // namespace sand::caf

UFW_DECLARE_MANAGED_DATA(sand::caf::caf_wrapper);

#endif // SANDRECO_CAF_WRAPPER_HPP
