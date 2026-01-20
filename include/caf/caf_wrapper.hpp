//
// Created by Paolo Forni on 11/14/25.
//

#ifndef SANDRECO_CAF_WRAPPER_HPP
#define SANDRECO_CAF_WRAPPER_HPP

#include <ufw/data.hpp>

#include <duneanaobj/StandardRecord/StandardRecord.h>

#include <map>
#include <memory>

namespace sand::caf {

  struct caf_wrapper
    : public ::caf::StandardRecord
    , public ufw::data::base<ufw::data::complex_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    caf_wrapper() = default;
  };

} // namespace sand::caf

UFW_DECLARE_COMPLEX_DATA(sand::caf::caf_wrapper);

// Factory for caf_wrapper - manages instances per context
template <>
class ufw::data::factory<sand::caf::caf_wrapper> {
 public:
  explicit factory(const ufw::config&) {}

  sand::caf::caf_wrapper& instance(ufw::context_id ctx) {
    auto it = instances_.find(ctx);
    if (it == instances_.end()) {
      instances_[ctx] = std::make_unique<sand::caf::caf_wrapper>();
      it = instances_.find(ctx);
    }
    return *it->second;
  }

 private:
  std::map<ufw::context_id, std::unique_ptr<sand::caf::caf_wrapper>> instances_;
};

#endif // SANDRECO_CAF_WRAPPER_HPP
