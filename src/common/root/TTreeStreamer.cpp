
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

  void TTreeStreamer::configure(const ufw::config& cfg) {
    streamer::configure(cfg);
    std::string openmode = cfg.value("mode", "READ");
    if (openmode == "READ") {
      m_mode = iop::ro;
    } else if (openmode == "RECREATE" || openmode == "CREATE" || openmode == "NEW") {
      m_mode = iop::wo;
    } else if (openmode == "UPDATE") {
      m_mode = iop::rw;
    } else {
      UFW_ERROR("Mode {} is not supported by TTreeStreamer", openmode);
    }
    m_file = new TFile(path().c_str(), openmode.c_str());
    if (!m_file->IsOpen() || m_file->IsZombie()) {
      UFW_ERROR("File {} could not be opened", path().string());
    }
    std::string treename = cfg.at("tree");
    if (m_mode == iop::ro || m_mode == iop::rw) {
      m_tree = m_file->Get<TTree>(treename.c_str());
    } else {
      m_tree = new TTree(treename.c_str(), "");
    }
  }

  void TTreeStreamer::attach(const ufw::type_id& t, ufw::data::data_base& d) {
    UFW_DEBUG("Attached object at {} of type {}", fmt::ptr(&d), t);
    TClass* tcl = TClass::GetClass(t.c_str());
    if (!tcl) {
      UFW_ERROR("TClass for '{}' not found.", t);
    }
    //you would think using a temporary here would be fine, and yet...
    m_branchaddr = &d;
    //remove any namespace from the branch name, for convenience.
    const char* brname = t.c_str();
    auto idx = t.find_last_of(':');
    if (idx != std::string::npos) {
      brname += idx + 1;
    }
    if (m_mode == iop::ro || m_mode == iop::rw) {
      TBranch* br = m_tree->GetBranch(brname);
      if (!br) {
        UFW_ERROR("TBranch '{}' not found.", brname);
      }
      br->SetAddress(&m_branchaddr);
    } else if (m_mode == iop::wo) {
      m_tree->Branch(brname, t.c_str(), &m_branchaddr);
    }
  }

  ufw::streamer::iop TTreeStreamer::support(const ufw::type_id& t) const {
    if (ufw::data::is_complex_type(t) || ufw::data::is_global_type(t)) {
      return iop::none;
    }
    TClass* tcl = TClass::GetClass(t.c_str());
    return tcl ? m_mode : iop::none;
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
