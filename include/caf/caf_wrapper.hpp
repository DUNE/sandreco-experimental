//
// Created by Paolo Forni on 11/14/25.
//

#ifndef SANDRECO_CAF_WRAPPER_HPP
#define SANDRECO_CAF_WRAPPER_HPP

#include <ufw/data.hpp>

#include <duneanaobj/StandardRecord/StandardRecord.h>

namespace sand::caf {

struct caf_wrapper
    : public ::caf::StandardRecord,
      public ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag,
                             ufw::data::context_tag> {};

} // namespace sand

UFW_DECLARE_MANAGED_DATA(sand::caf::caf_wrapper);

#endif // SANDRECO_CAF_WRAPPER_HPP
