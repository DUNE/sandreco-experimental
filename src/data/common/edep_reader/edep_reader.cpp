#include <TFile.h>
#include <TTree.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/data.hpp>

#include <edep_reader/edep_reader.hpp>

namespace sand {
  truth_adapter::value_type& truth_adapter::at(const index_type&) { UFW_FATAL("Not yet implemented"); }

  bool truth_adapter::valid(const index_type&) { UFW_FATAL("Not yet implemented"); }

} // namespace sand

ufw::data::factory<sand::edep_reader>::factory(const ufw::config& cfg) : input_file(nullptr), event(new TG4Event()) {
  auto path = cfg.path_at("uri");
  input_file.reset(TFile::Open(path.c_str()));
  input_tree = input_file->Get<TTree>("EDepSimEvents");
  if (!input_tree) {
    UFW_ERROR("EDepSim tree not found in file '{}'.", path.c_str());
  }
  TBranch* br = input_tree->GetBranch("Event");
  if (!br) {
    UFW_ERROR("EDepSim branch \"Event\" not found in file '{}'.", path.c_str());
  }
  br->SetAddress(&event);

  const auto entries_n = input_tree->GetEntries();
  size_t total_events = 0;
  for (size_t i = 0; i < entries_n; i++) {
    input_tree->GetEntry(i);
    const auto event_number = event->Primaries.size();
    total_events += event_number;
    UFW_DEBUG("Spill {} has {} events", i, event_number);
  }
  UFW_DEBUG("total events number: {}", total_events);
}

ufw::data::factory<sand::edep_reader>::~factory() = default;

sand::edep_reader& ufw::data::factory<sand::edep_reader>::instance(ufw::context_id i) {
  if (m_id != i) {
    input_tree->GetEntry(i);
    reader.InizializeFromEdep(*event);
    reader.m_event = event;
    m_id           = i;
  }
  return reader;
}
