#include <ufw/data.hpp>
#include <ufw/config.hpp>

#include <EdepReader.hpp>


namespace sand {
  EdepReader::EdepReader() : EDEPTree() {}
}

ufw::data::factory<sand::EdepReader>::factory(const ufw::config& cfg) :
  input_file(TFile::Open(cfg.at("uri").template get<std::string>().c_str())),
  event(new TG4Event()) {
  
    UFW_INFO("Crated file {} at {} in {}", "input_file", fmt::ptr(input_file), cfg.at("uri").template get<std::string>().c_str());


  input_tree = input_file->Get<TTree>("EDepSimEvents");
	
  if(!input_tree){
		UFW_ERROR("EDepSim tree not found!");
	}

  input_tree->SetBranchAddress("Event", &event);
}

sand::EdepReader& ufw::data::factory<sand::EdepReader>::instance(ufw::context_id i) {
  input_tree->GetEntry(i);
  reader.InizializeFromEdep(*event);
  return reader;
}