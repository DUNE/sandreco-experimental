#include <TFile.h>
#include <TTree.h>

#include <ufw/data.hpp>
#include <ufw/config.hpp>

#include <edep_reader/edep_reader.hpp>


namespace sand {
  edep_reader::edep_reader() : EDEPTree() {}
}

ufw::data::factory<sand::edep_reader>::factory(const ufw::config& cfg) :
  input_file(TFile::Open(cfg.at("uri").template get<std::string>().c_str())),
  event(new TG4Event()) {
  
    UFW_INFO("Crated file {} at {} in {}", "input_file", fmt::ptr(input_file), cfg.at("uri").template get<std::string>().c_str());


  input_tree = input_file->Get<TTree>("EDepSimEvents");
	
  if(!input_tree){
		UFW_ERROR("EDepSim tree not found!");
	}

  input_tree->SetBranchAddress("Event", &event);
}

sand::edep_reader& ufw::data::factory<sand::edep_reader>::instance(ufw::context_id i) {
  if (m_id != i) {
    input_tree->GetEntry(i);
    reader.InizializeFromEdep(*event);
    m_id = i;
  }
  return reader;
}
