#ifndef SAND_SPILL_MAPS_HPP
#define SAND_SPILL_MAPS_HPP

#include "g4_id_map.hpp"

#include <common/data.h>

#include <cstddef>
#include <vector>

namespace sand {

  /// The G4IdMap of every interaction of the current spill, indexed like SRTruthBranch::nu.
  ///
  /// Framework data, one per spill: get it with `instance<sand::spill_maps>()`. Needs `sand::edep_reader`.
  class spill_maps : public ufw::data::base<ufw::data::complex_tag, ufw::data::unique_tag, ufw::data::context_tag> {
    std::vector<G4IdMap> m_maps;

    spill_maps() = default;
    friend class ufw::data::factory<spill_maps>;

   public:
    G4IdMap const& at(std::size_t ixn) const { return m_maps.at(ixn); }
    std::size_t size() const noexcept { return m_maps.size(); }
  };

} // namespace sand

UFW_DECLARE_COMPLEX_DATA(sand::spill_maps);

template <>
class ufw::data::factory<sand::spill_maps> {
 public:
  explicit factory(ufw::config const&) {}
  sand::spill_maps& instance(ufw::context_id id);

 private:
  sand::spill_maps m_maps;
  ufw::context_id m_id;
};

#endif
