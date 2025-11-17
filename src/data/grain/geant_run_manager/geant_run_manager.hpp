#include <ufw/data.hpp>

#include <G4RunManager.hh>

namespace sand::grain {

  struct geant_run_manager : public G4RunManager, public ufw::data::base<ufw::data::complex_tag, 
                                                                         ufw::data::unique_tag, 
                                                                         ufw::data::global_tag> {
  public:
    explicit geant_run_manager(const ufw::config&);

  };

}

UFW_DECLARE_COMPLEX_DATA(sand::grain::geant_run_manager);
