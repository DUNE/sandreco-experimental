#pragma once

class TFile;
class TTree;

#include <ufw/streamer.hpp>

namespace sand::common::root {

  class tree_streamer : public ufw::streamer {

  public:
    ~tree_streamer();

    void configure(const ufw::config&, const ufw::type_id&, ufw::op_type) override;

    void attach(ufw::data::data_base&) override;

    void read(ufw::context_id) override;

    void write(ufw::context_id) override;

  private:
    TFile* m_file;
    TTree* m_tree;
    void* m_branchaddr;;

  };

}

UFW_REGISTER_STREAMER(sand::common::root::tree_streamer)
