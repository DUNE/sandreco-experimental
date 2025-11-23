#pragma once

class TFile;
class TTree;

#include <ufw/streamer.hpp>

namespace sand::root {

  class tree_streamer : public ufw::streamer {
   public:
    tree_streamer();

    ~tree_streamer();

    void configure(const ufw::config&, ufw::op_type) override;

    void prepare(const ufw::public_id&, const ufw::type_id&) override;

    void attach(ufw::data::data_base&, const ufw::public_id&) override;

    void read(ufw::context_id) override;

    void write(ufw::context_id) override;

   private:
    std::unique_ptr<TFile> m_file;
    TTree* m_tree;
    void* m_branchaddr;
    ufw::context_id m_id;
    ufw::context_id* m_id_ptr;
    long m_last_entry;
    static constexpr char s_id_brname[] = "context_id";
  };

} // namespace sand::root

UFW_REGISTER_STREAMER(sand::root::tree_streamer)
