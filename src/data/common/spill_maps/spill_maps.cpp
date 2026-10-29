#include "spill_maps.hpp"

#include <edep_reader/edep_reader.hpp>

#include <ufw/context.hpp>

sand::spill_maps& ufw::data::factory<sand::spill_maps>::instance(ufw::context_id id) {
  if (m_id != id) {
    auto const& edep = ufw::context::current()->instance<sand::edep_reader>();
    m_maps.m_maps    = sand::g4_id_maps(edep.GetChildrenTrajectories());
    m_id             = id;
  }
  return m_maps;
}
