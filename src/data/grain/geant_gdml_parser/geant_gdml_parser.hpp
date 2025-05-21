#include <ufw/data.hpp>

#include <G4GDMLParser.hh>

namespace sand::grain {

  struct geant_gdml_parser : public G4GDMLParser, public ufw::data::base<ufw::data::complex_tag, 
                                                                         ufw::data::instanced_tag,
                                                                         ufw::data::global_tag> {
  public:
    explicit geant_gdml_parser(const ufw::config&);

  };

}

UFW_DECLARE_COMPLEX_DATA(sand::grain::geant_gdml_parser);
