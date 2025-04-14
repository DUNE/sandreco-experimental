#include <ufw/data.hpp>
#include <ufw/utils.hpp>

#include <G4RunManager.hh>
#include "G4UImanager.hh"
#include "G4GDMLParser.hh"
#include "G4VisExecutive.hh"
#include "G4UIExecutive.hh"

#include <G4_optmen_runmanager.hpp>

G4_optmen_runmanager::G4_optmen_runmanager(const ufw::config& cfg) {

}

void G4_optmen_runmanager::setOutputs(const ufw::var_id_map& outputs) {
  m_outputs = outputs;
}