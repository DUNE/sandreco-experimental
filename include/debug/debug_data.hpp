#ifndef DEBUG_INFO_HPP
#define DEBUG_INFO_HPP

#include <string>
#include <vector>

#include <ufw/data.hpp>
#include <edep_reader/EDEPHit.h>

namespace sand::debug {

struct debug_data : public ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
  std::vector<std::vector<EDEPHit>> hits = {};

  void clear() {
    hits.clear();
  }
};

} // namespace sand::debug

UFW_REGISTER_TYPE_NAME(sand::debug::debug_data, "sand::debug::debug_data")
UFW_DECLARE_RTTI(sand::debug::debug_data)

#endif // DEBUG_INFO_HPP
