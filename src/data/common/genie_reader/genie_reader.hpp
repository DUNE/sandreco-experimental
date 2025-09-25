#pragma once

#include <ufw/data.hpp>

#include <common/truth.h>

#include <genie_reader/GenieWrapper.h>

#include "TObjString.h"
#include "TBits.h"

class TFile;
class TTree;

namespace sand {

  struct genie_reader : public GenieWrapper, public ufw::data::base<ufw::data::complex_tag,
                                               ufw::data::unique_tag,
                                               ufw::data::context_tag> {

    private:
      genie_reader();
      friend class ufw::data::factory<sand::genie_reader>;

  };

}

UFW_DECLARE_COMPLEX_DATA(sand::genie_reader);

template <>
class ufw::data::factory<sand::genie_reader> {
  public:
    factory(const ufw::config&);
    ~factory();
    sand::genie_reader& instance(ufw::context_id);

  private:
    sand::genie_reader reader;
    std::unique_ptr<TFile> input_file;
    TTree* input_tree;
    ufw::context_id m_id;

    int EvtNum;
    double EvtXSec;
    double EvtDXSec;
    double EvtKPS;
    double EvtWght;
    double EvtProb;
    double EvtVtx[4];
    TObjString* EvtCode;
    TBits* EvtFlags;
};
