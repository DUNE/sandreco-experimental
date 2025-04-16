#include <filesystem>

#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <grain/photons.h>
#include <optical_simulation.hpp>
#include <edep_reader/edep_reader.hpp>

#include "DetectorConstruction.hh"
#include "ActionInitialization.hh"
#include "AnalysisManager.hh"
#include "PhysicsList.hh"


#include <geant_run_manager/geant_run_manager.hpp>

#include "Randomize.hh"
#include "TSystem.h"

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::optical_simulation)

namespace sand::grain {

void optical_simulation::configure (const ufw::config& cfg) {
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
  

  auto& run_manager = ufw::context::instance<geant_run_manager>();
  UFW_INFO("Accessed instance of run manager at: {}", fmt::ptr(&run_manager));

  run_manager.SetUserInitialization(new DetectorConstruction(parser, this));

  run_manager.SetUserInitialization(new PhysicsList);
  
  // Set user action classes
  AnalysisManager *pAnalysisManager = new AnalysisManager(this);
  run_manager.SetUserInitialization(new ActionInitialization(pAnalysisManager, this));

  run_manager.Initialize();
}
  
optical_simulation::optical_simulation() : process({}, {{"cameras_out", "sand::grain::hits"}}) {
  UFW_INFO("Creating a optical_simulation process at {}", fmt::ptr(this));
}


void optical_simulation::run(const ufw::var_id_map& inputs, const ufw::var_id_map& outputs) {
  // CLHEP::HepRandom::setTheSeed(m_seed);
  // CLHEP::HepRandom::showEngineStatus();
  m_output_variable_name = outputs.at("cameras_out");
  m_run_start = true;
  m_new_iteration = true;

  auto& run_manager = ufw::context::instance<geant_run_manager>();

  UFW_INFO("Accessed instance of run manager at: {}", fmt::ptr(&run_manager));
  run_manager.BeamOn(GetEventsNumber());
}  


int optical_simulation::GetEventsNumber() {
  UFW_DEBUG("Computing the number of block for the event");
	auto& tree = ufw::context::instance<sand::edep_reader>();
  int eventCount = 0;

	for (auto trj_it = tree.begin(); trj_it != tree.end(); trj_it++) {

		if (trj_it->GetHitMap().find(component::GRAIN) != trj_it->GetHitMap().end()) {
      eventCount += trj_it->GetHitMap().at(component::GRAIN).size();
    }
	}
  UFW_DEBUG("Split into {} events.", eventCount);

	return eventCount;
}
}
