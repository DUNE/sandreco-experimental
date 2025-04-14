#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/utils.hpp>

#include <G4RunManager.hh>
#include "G4UImanager.hh"
#include "G4GDMLParser.hh"
#include "G4VisExecutive.hh"
#include "G4UIExecutive.hh"

struct G4_optmen_runmanager : public G4RunManager, public ufw::data::base<ufw::data::complex_tag, 
                                                                          ufw::data::singleton_tag, 
                                                                          ufw::data::global_tag> {

  public:
    explicit G4_optmen_runmanager(const ufw::config&);

    void setOutputs(const ufw::var_id_map& outputs);
    const ufw::var_id_map& getOutputs() {return m_outputs;};

  private:
    ufw::var_id_map m_outputs;
};

UFW_DECLARE_COMPLEX_DATA(G4_optmen_runmanager);