#pragma once

class TFile;
class TTree;

#include <streamer.hpp>

namespace sand::common::root {


  class TTreeStreamer : public ufw::streamer {

  public:
    ~TTreeStreamer();

    void configure(const ufw::config&) override;

    void attach(const ufw::type_id&, ufw::data::data_base&) override;

    iop support(const ufw::type_id&) const override;

    void read(ufw::context_id) override;

    void write(ufw::context_id) override;

    ufw::streamer::iop mode() const { return m_mode; }

  private:
    TFile* m_file;
    TTree* m_tree;
    void* m_branchaddr;
    ufw::streamer::iop m_mode = iop::none;

  };

}

UFW_REGISTER_STREAMER(sand::common::root::TTreeStreamer)
