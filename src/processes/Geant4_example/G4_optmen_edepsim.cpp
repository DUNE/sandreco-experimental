#include <filesystem>

#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <grain/photons.h>
#include <G4_optmen_edepsim.hpp>
#include <EdepReader/EdepReader.hpp>

#include "OptMenDetectorConstruction.hh"
#include "OptMenActionInitialization.hh"
#include "OptMenAnalysisManager.hh"
#include "OptMenPhysicsList.hh"


#include <G4_optmen_runmanager/G4_optmen_runmanager.hpp>
#include <root/TTreeStreamer.hpp>

#include "Randomize.hh"
#include "TSystem.h"

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(G4_optmen_edepsim)

void G4_optmen_edepsim::configure (const ufw::config& cfg) {
  process::configure(cfg);
  
  m_energy_split_threshold = cfg.value("energy_split_threshold", m_energy_split_threshold);
  m_geometry = cfg.at("geometry").template get<std::string>().c_str();
  
  if (m_geometry.string().find("lenses") != std::string::npos) {
    m_optics_type = OpticsType::LENS;
    if (m_geometry.string().find("Xe") != std::string::npos) {
      m_optics_type = OpticsType::LENS_DOPED;
    }
  } else {
    m_optics_type = OpticsType::MASK;
  }
  UFW_INFO("Optics type {}", m_optics_type);
  
  auto starting_path = std::filesystem::current_path();
  UFW_DEBUG("Starting path {}", std::filesystem::current_path().string());
  std::filesystem::current_path(m_geometry.parent_path());
  UFW_DEBUG("Current path {}", std::filesystem::current_path().string());
  G4GDMLParser parser;
  parser.SetOverlapCheck(true);
  parser.SetStripFlag(false);
  UFW_DEBUG("Reading geometry file: {}", m_geometry.filename().string());
  parser.Read(m_geometry.filename().c_str(), false);
  std::filesystem::current_path(starting_path);
  UFW_DEBUG("Back to {}", std::filesystem::current_path().string());
  

  auto& run_manager = ufw::context::instance<G4_optmen_runmanager>();
  UFW_INFO("Accessed instance of run manager at: {}", fmt::ptr(&run_manager));

  run_manager.SetUserInitialization(new OptMenDetectorConstruction(parser, this));

  run_manager.SetUserInitialization(new OptMenPhysicsList);
  
  // Set user action classes
  OptMenAnalysisManager *pAnalysisManager = new OptMenAnalysisManager(this);
  run_manager.SetUserInitialization(new OptMenActionInitialization(pAnalysisManager, this));

  run_manager.Initialize();
}
  
G4_optmen_edepsim::G4_optmen_edepsim() : process({}, {{"cameras_out", "sand::grain::hits"}}) {
  UFW_INFO("Creating a G4_optmen_edepsim process at {}", fmt::ptr(this));
}


void G4_optmen_edepsim::run(const ufw::var_id_map& inputs, const ufw::var_id_map& outputs) {
  // CLHEP::HepRandom::setTheSeed(m_seed);
  // CLHEP::HepRandom::showEngineStatus();
  m_output_variable_name = outputs.at("cameras_out");
  m_run_start = true;
  m_new_iteration = true;

  auto& run_manager = ufw::context::instance<G4_optmen_runmanager>();

  UFW_INFO("Accessed instance of run manager at: {}", fmt::ptr(&run_manager));
  run_manager.BeamOn(GetEventsNumber());
}  


int G4_optmen_edepsim::GetEventsNumber() {
  UFW_DEBUG("Computing the number of block for the event");
	auto& tree = ufw::context::instance<sand::EdepReader>();
  int eventCount = 0;

	for (auto trj_it = tree.begin(); trj_it != tree.end(); trj_it++) {

		if (trj_it->GetHitMap().find(component::GRAIN) != trj_it->GetHitMap().end()) {
      eventCount += trj_it->GetHitMap().at(component::GRAIN).size();
    }
	}
  UFW_DEBUG("Split into {} events.", eventCount);

	return eventCount;
}