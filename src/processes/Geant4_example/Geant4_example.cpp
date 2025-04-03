#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <root/TTreeStreamer.hpp>

#include <G4RunManager.hh>
#include "G4UImanager.hh"
#include "G4GDMLParser.hh"
#include "G4VisExecutive.hh"
#include "G4UIExecutive.hh"

#include "Randomize.hh"
#include "TSystem.h"


class Geant4_example : public ufw::process {

  public:
  Geant4_example();
  void configure (const ufw::config& cfg) override;
  // const ufw::var_type_map& products() const override;
  // const ufw::var_type_map& requirements() const override;
  void run(const ufw::var_id_map& inputs, const ufw::var_id_map& outputs) override;

  
  private:
    int m_seed = 0;
  };
  
  UFW_REGISTER_PROCESS(Geant4_example)
  UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(Geant4_example)

  void Geant4_example::configure (const ufw::config& cfg) {
    process::configure(cfg);
    m_seed = cfg.at("seed");
  }
  
  // ufw::data_list Geant4_example::products() const {
  //   return ufw::data_list{{"output", "sand::example"}};
  // }
  
  // ufw::data_list Geant4_example::requirements() const {
  //   return ufw::data_list{{"input", "sand::example"}};
  // }
  
  Geant4_example::Geant4_example() : process({}, {}) {
    UFW_INFO("Creating a Geant4_example process at {}", fmt::ptr(this));
  }


  void Geant4_example::run(const ufw::var_id_map& inputs, const ufw::var_id_map& outputs) {
    // G4RunManager *runManager = new G4RunManager;
    CLHEP::HepRandom::setTheSeed(m_seed);
    CLHEP::HepRandom::showEngineStatus();

    CLHEP::HepRandom::setTheSeed(ufw::context::current());
    CLHEP::HepRandom::showEngineStatus();
    
    // delete runManager;
  }
  