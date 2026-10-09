#include <TFile.h>
#include <TTree.h>

#include <edep_reader_refactor/edep_reader_refactor.hpp>

#include <ufw/config.hpp>
#include <ufw/context.hpp>

ufw::data::factory<sand::edep_reader_refactor>::factory(const ufw::config& cfg)
  : input_file(nullptr), input_tree(nullptr), event_owner(new TG4Event()), event(event_owner.get()) {
  auto path = cfg.path_at("uri");
  input_file.reset(TFile::Open(path.c_str()));
  input_tree = input_file ? input_file->Get<TTree>("EDepSimEvents") : nullptr;
  if (!input_tree) {
    UFW_ERROR("EDepSim tree not found in file '{}'.", path.c_str());
  }
  TBranch* br = input_tree->GetBranch("Event");
  if (!br) {
    UFW_ERROR("EDepSim branch \"Event\" not found in file '{}'.", path.c_str());
  }
  br->SetAddress(&event);
}

ufw::data::factory<sand::edep_reader_refactor>::~factory() = default;

sand::edep_reader_refactor& ufw::data::factory<sand::edep_reader_refactor>::instance(ufw::context_id i) {
  if (m_id != i) {
    input_tree->GetEntry(i);
    reader.InizializeFromEdep(*event);
    reader.m_event = event;
    m_id           = i;
  }
  return reader;
}
