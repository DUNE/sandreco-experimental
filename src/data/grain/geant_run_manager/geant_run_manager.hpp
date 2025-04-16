#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/utils.hpp>

#include <G4RunManager.hh>
#include "G4UImanager.hh"
#include "G4GDMLParser.hh"
#include "G4VisExecutive.hh"
#include "G4UIExecutive.hh"

struct geant_run_manager : public G4RunManager, public ufw::data::base<ufw::data::complex_tag, 
                                                                          ufw::data::singleton_tag, 
                                                                          ufw::data::global_tag> {
  public:
    explicit geant_run_manager(const ufw::config&);
};

UFW_DECLARE_COMPLEX_DATA(geant_run_manager);