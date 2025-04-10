#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include "OptMenDetectorConstruction.hh"
#include "OptMenActionInitialization.hh"
#include "OptMenAnalysisManager.hh"
#include "OptMenPhysicsList.hh"
#include "OptMenReadParameters.hh"
#include "OptMenEventCounter.hh"

#include <G4_optmen_runmanager/G4_optmen_runmanager.hpp>
#include <root/TTreeStreamer.hpp>

#include "Randomize.hh"
#include "TSystem.h"


class G4_optmen_edepsim : public ufw::process {

  public:
  G4_optmen_edepsim();
  void configure (const ufw::config& cfg) override;
  // const ufw::var_type_map& products() const override;
  // const ufw::var_type_map& requirements() const override;
  void run(const ufw::var_id_map& inputs, const ufw::var_id_map& outputs) override;

  
  private:
    int m_seed = 0;
    G4String inputFile;
};
  
  UFW_REGISTER_PROCESS(G4_optmen_edepsim)
  UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(G4_optmen_edepsim)

  void G4_optmen_edepsim::configure (const ufw::config& cfg) {
    process::configure(cfg);
    m_seed = cfg.at("seed");
    std::ifstream ifs(cfg.at("config"));
  	OptMenReadParameters::Get()->ReadConfigurationFile(ifs);
	  ifs.close();

    std::string startingPath = gSystem->pwd();
    
    std::string geomPath = OptMenReadParameters::Get()->GetGeometryFile();
    gSystem->cd(gSystem->DirName(geomPath.c_str()));

    inputFile = OptMenReadParameters::Get()->GetInputFile();

    G4GDMLParser parser;
    parser.SetOverlapCheck(true);
    parser.SetStripFlag(false);
    parser.Read(gSystem->BaseName(geomPath.c_str()), false);

    gSystem->cd(startingPath.c_str());

    auto& run_manager = ufw::context::instance<G4_optmen_runmanager>();
    UFW_INFO("Accessed instance of run manager at: {}", fmt::ptr(&run_manager));

    run_manager.SetUserInitialization(new OptMenDetectorConstruction(parser));

    OptMenAnalysisManager *pAnalysisManager = new OptMenAnalysisManager();
    run_manager.SetUserInitialization(new OptMenPhysicsList);

    // Set user action classes
    run_manager.SetUserInitialization(new OptMenActionInitialization(pAnalysisManager));

    run_manager.Initialize();
  }
  
  // ufw::data_list G4_optmen_edepsim::products() const {
  //   return ufw::data_list{{"output", "sand::example"}};
  // }
  
  // ufw::data_list G4_optmen_edepsim::requirements() const {
  //   return ufw::data_list{{"input", "sand::example"}};
  // }
  
  G4_optmen_edepsim::G4_optmen_edepsim() : process({}, {{"cameras", "sand::grain::photons"}}) {
    UFW_INFO("Creating a G4_optmen_edepsim process at {}", fmt::ptr(this));
  }


  void G4_optmen_edepsim::run(const ufw::var_id_map& inputs, const ufw::var_id_map& outputs) {
    // CLHEP::HepRandom::setTheSeed(m_seed);
    // CLHEP::HepRandom::showEngineStatus();

    UFW_INFO("Requested {} events", OptMenReadParameters::Get()->GetEventNumber());
    OptMenEventCounter eventCounter;
    OptMenReadParameters::Get()->SetSplitEventNumber(eventCounter.GetEventsCount());
    UFW_INFO("{} events being processed instead.", OptMenReadParameters::Get()->GetSplitEventNumber());

    if (OptMenReadParameters::Get()->GetIDlistName() != "") {
      std::ifstream idIfs(OptMenReadParameters::Get()->GetIDlistName());
      std::string idLine;
      while(getline(idIfs, idLine)) {
        if(idLine.empty() || idLine[0] == '#') continue;
        OptMenReadParameters::Get()->pushIdList(std::stoi(idLine));
      }
    }

    auto& run_manager = ufw::context::instance<G4_optmen_runmanager>();
    UFW_INFO("Accessed instance of run manager at: {}", fmt::ptr(&run_manager));
    run_manager.BeamOn(OptMenReadParameters::Get()->GetSplitEventNumber());

      
}  
