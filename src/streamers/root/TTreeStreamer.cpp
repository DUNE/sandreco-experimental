
#include <TFile.h>
#include <TTree.h>

#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>

#include <TTreeStreamer.hpp>

#define UFW_IMPLEMENT_STREAMER_FOR_TYPE(type) \
UFW_DECLARE_RTTI(type)
#include <TTreeStreamerTypes.hpp>
#undef UFW_IMPLEMENT_STREAMER_FOR_TYPE

namespace sand::common::root {

  TTreeStreamer::~TTreeStreamer() {
    m_file->cd();
    m_tree->Write();
    m_file->Close();
    //delete m_tree; //deleted in close
    delete m_file;
  }

  void TTreeStreamer::configure(const ufw::config& cfg, const ufw::type_id& tp, ufw::op_type op) {
    streamer::configure(cfg, tp, op);
    TClass* tcl = TClass::GetClass(tp.c_str());
    if (!tcl) {
      UFW_ERROR("TClass for '{}' not found.", tp);
    }
    const char* opmode = "";
    switch (op) {
    case ufw::op_type::ro:
      opmode = "READ";
      break;
    case ufw::op_type::wo:
      opmode = "RECREATE";
      break;
    case ufw::op_type::rw:
      opmode = "UPDATE";
      break;
    default:
      UFW_ERROR("Mode {} is not supported by TTreeStreamer", op);
      break;
    };
    m_file = new TFile(path().c_str(), opmode);
    if (!m_file->IsOpen() || m_file->IsZombie()) {
      UFW_ERROR("File {} could not be opened", path().string());
    }
    std::string treename = cfg.at("tree");
    if (op & ufw::op_type::ro) {
      m_tree = m_file->Get<TTree>(treename.c_str());
    } else {
      m_tree = new TTree(treename.c_str(), "");
    }
  }

  void TTreeStreamer::attach(ufw::data::data_base& d) {
    //you would think using a temporary here would be fine, and yet...
    m_branchaddr = &d;
    //remove any namespace from the branch name, for convenience.
    auto brname = ufw::simplified_name(type());
    if (operation() & ufw::op_type::ro) {
      TBranch* br = m_tree->GetBranch(brname.c_str());
      if (!br) {
        UFW_ERROR("TBranch '{}' not found.", brname);
      }
      br->SetAddress(&m_branchaddr);
    } else if (operation() == ufw::op_type::wo) {
      m_tree->Branch(brname.c_str(), type().c_str(), &m_branchaddr);
    }
    streamer::attach(d);
  }

  void TTreeStreamer::read(ufw::context_id i) {
    m_file->cd();
    m_tree->GetEntry(i);
  }

  void TTreeStreamer::write(ufw::context_id) {
    m_file->cd();
    m_tree->Fill();
  }

}

UFW_REGISTER_DYNAMIC_STREAMER_FACTORY(sand::common::root::TTreeStreamer)
