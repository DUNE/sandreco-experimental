#include <ufw/data.hpp>

#include "EDEPTree.h"
#include <TFile.h>        // sorry Nico
#include <TTreeReader.h>  // sorry Nico
#include <TTree.h>        // sorry Nico


class TG4Event;
// class TFile;
// class TTree;

namespace sand {
  struct EdepReader : public EDEPTree, public ufw::data::base<ufw::data::complex_tag, 
                                              ufw::data::singleton_tag, 
                                              ufw::data::context_tag> {

    public:
      explicit EdepReader();
  };
}

UFW_DECLARE_COMPLEX_DATA(sand::EdepReader);

template <>
class ufw::data::factory<sand::EdepReader> {
  public:
    factory(const ufw::config&);
    sand::EdepReader& instance(ufw::context_id);

  private:
    sand::EdepReader reader;
    std::unique_ptr<TFile> input_file;
    TTree* input_tree;
    TG4Event* event;
  
};


