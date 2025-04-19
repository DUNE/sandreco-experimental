#pragma once

#include <ufw/data.hpp>

#include <edep_reader/EDEPTree.h>

class TG4Event;
class TFile;
class TTree;

namespace sand {
  struct edep_reader : public EDEPTree, public ufw::data::base<ufw::data::complex_tag, 
                                              ufw::data::singleton_tag, 
                                              ufw::data::context_tag> {

    public:
      explicit edep_reader();
  };
}

UFW_DECLARE_COMPLEX_DATA(sand::edep_reader);

template <>
class ufw::data::factory<sand::edep_reader> {
  public:
    factory(const ufw::config&);
    ~factory();
    sand::edep_reader& instance(ufw::context_id);

  private:
    sand::edep_reader reader;
    std::unique_ptr<TFile> input_file;
    TTree* input_tree;
    TG4Event* event;
    ufw::context_id m_id;
  
};


